/*
	Transporter is a ZMQ based file transfer utility
	Copyright (C) 2020 Brett Kuskie <fullaxx@gmail.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "async_zmq_reply.h"
#include "transporter.h"
#include "futils.h"
#include "tpad_error.h"

typedef struct dirent dir_t;

/*
int hidden_file_check(const dir_t *entry)
{
	// Do not count hidden files
	if(entry->d_name[0] == '.') return 0;

	// default accept
	return 1;
}
*/

static int filter(const dir_t *entry)
{
	if(is_regfile(entry->d_name, 0) == 1) return 1;

	// default deny
	return 0;
}

static int count_files(char *dir)
{
	int entries;
	int retval = 0;
	dir_t **mod_dirent = NULL;	// our array of directory entries (used for searching dir for modules)

	//scans PLUGIN_PATH for all files...then used the function mod_fname_check to filter
	//then the alpha sort function to sort, and puts the result in g_mod_dirent
	entries = scandir(dir, &mod_dirent, filter, alphasort);
	if(entries < 0) { return -1; }

	retval = entries;

	//Loop to free all dir entries back to the heap since scandir made malloc calls
	if(entries > 0) {
		while(entries--) {
			free(mod_dirent[entries]);
		}
		free(mod_dirent);
	}

	return retval;
}

static void do_count(zmq_reply_t *r)
{
	int n, filecount;
	char resp[128];

	filecount = count_files(".");
	n = snprintf(resp, sizeof(resp), "%d", filecount);
	(void) as_zmq_reply_send(r, TCMD_OK, strlen(TCMD_OK)+1, 1);
	(void) as_zmq_reply_send(r, resp, n+1, 0);
}

static char* pick_rand_file(char *dir)
{
	int entries;
	char *rf = NULL;
	dir_t **mod_dirent = NULL;	// our array of directory entries (used for searching dir for modules)

	//scans PLUGIN_PATH for all files...then used the function mod_fname_check to filter
	//then the alpha sort function to sort, and puts the result in g_mod_dirent
	entries = scandir(dir, &mod_dirent, filter, alphasort);
	if(entries < 0) { return NULL; }

	//Loop to free all dir entries back to the heap since scandir made malloc calls
	if(entries > 0) {
		rf = strdup(mod_dirent[0]->d_name);
		while(entries--) {
			free(mod_dirent[entries]);
		}
		free(mod_dirent);
	}

	return rf;
}

static void get_file(zmq_reply_t *r, char *filename, long max_len)
{
	char errmsg[1400];
	char filesize[24];
	long size;
	FILE *f;
	char *buf;
	int n;

	size = file_size(filename, 1);
	if(size < 0) {
		snprintf(errmsg, sizeof(errmsg), "%s: filesize(%ld) < 0", filename, size);
		tpad_error(r, __func__, errmsg);
		return;
	}

	// We are going to set a filesize limit of 2G since we are going to malloc the whole thing
	if(size > MAXFILESIZE) {
		snprintf(errmsg, sizeof(errmsg), "%s: filesize(%ld) > MAXFILESIZE(%ld)", filename, size, MAXFILESIZE);
		tpad_error(r, __func__, errmsg);
		return;
	}

	if(size > max_len) {
		snprintf(errmsg, sizeof(errmsg), "%s: filesize(%ld) > max_len(%ld)", filename, size, max_len);
		tpad_error(r, __func__, errmsg);
		return;
	}

	f = fopen(filename, "r");
	if(!f) {
		snprintf(errmsg, sizeof(errmsg), "fopen(%s, r) failed: %s", filename, strerror(errno));
		tpad_error(r, __func__, errmsg);
		return;
	}

	buf = malloc(size);
	if(!buf) {
		fclose(f);
		snprintf(errmsg, sizeof(errmsg), "malloc(%ld) failed", size);
		tpad_error(r, __func__, errmsg);
		return;
	}

	n = fread(buf, size, 1, f);
	if(n != 1) {
		fclose(f);
		free(buf);
		snprintf(errmsg, sizeof(errmsg), "fread(buf, %ld, 1, %s) failed", size, filename);
		tpad_error(r, __func__, errmsg);
		return;
	}

	snprintf(filesize, sizeof(filesize), "%lu", size);
	(void) as_zmq_reply_send(r, TCMD_OK, strlen(TCMD_OK)+1, 1);
	(void) as_zmq_reply_send(r, filename, strlen(filename)+1, 1);
	(void) as_zmq_reply_send(r, filesize, strlen(filesize)+1, 1);
	(void) as_zmq_reply_send(r, buf, size, 0);

	// XXX FIXME We should check errors here

	free(buf);
	fclose(f);
}

/* absorb.c
zmq_send(zGETReq, TCMD_GET,		strlen(TCMD_GET)+1,		ZMQ_SNDMORE);
zmq_send(zGETReq, RANDOMCMD,	strlen(RANDOMCMD)+1,	ZMQ_SNDMORE);
zmq_send(zGETReq, recvsize,		strlen(recvsize)+1,		0);
*/
void tpad_get_cb(zmq_reply_t *r, zmq_mf_t **mpa, int msgcnt, void *user_data)
{
	char *cmd;
	char *filename;
	char *rf;
	long maxlen;
	char errmsg[1400];
	zmq_mf_t *command_msg;	// Command (String)
	zmq_mf_t *filename_msg;	// Filename (String)
	zmq_mf_t *maxlen_msg;	// MAXLEN (String)

	if(!mpa) { return; }
	if(msgcnt != 3) { return; }

	command_msg = mpa[0];	if(!command_msg) return;
	filename_msg = mpa[1];	if(!filename_msg) return;
	maxlen_msg = mpa[2];	if(!maxlen_msg) return;

	cmd = (char *)command_msg->buf;
	if(strcmp(cmd, TCMD_GET) != 0) {
		snprintf(errmsg, sizeof(errmsg), "INVALID COMMAND: %s", cmd);
		tpad_error(r, __func__, errmsg);
		return;
	}

	filename = (char *)filename_msg->buf;
	maxlen = atol(maxlen_msg->buf);

	if(file_security_check(filename)) {
		snprintf(errmsg, sizeof(errmsg), "INVALID FILENAME: %s", filename);
		tpad_error(r, __func__, errmsg);
		return;
	}

	if(strcmp(filename, COUNTCMD) == 0) {
#ifdef DEBUG
		printf("%s(): %s %s\n", __func__, cmd, filename);
#endif
		do_count(r);
		return;
	}

	if(strcmp(filename, RANDOMCMD) == 0) {
#ifdef DEBUG
		printf("%s(): %s %s\n", __func__, cmd, filename);
#endif
		rf = pick_rand_file(".");
		if(!rf) {
			snprintf(errmsg, sizeof(errmsg), "ZERO FILES AVAILABLE");
			fprintf(stderr, "%s(): %s\n", __func__, cmd);
			(void) as_zmq_reply_send(r, TCMD_ERR, strlen(TCMD_ERR)+1, 1);
			(void) as_zmq_reply_send(r, errmsg, strlen(errmsg)+1, 0);
			return;
		}
		get_file(r, rf, maxlen);
		free(rf);
		return;
	}

#ifdef DEBUG
	printf("%s(): %s %s (%lu)\n", __func__, cmd, filename, maxlen);
#endif

	// If not built-in command, get a specific file
	get_file(r, filename, maxlen);
}
