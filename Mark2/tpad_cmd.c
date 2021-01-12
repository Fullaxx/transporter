/*
	Transporter is a ZMQ based file transfer utility
	Copyright (C) 2021 Brett Kuskie <fullaxx@gmail.com>

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
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "async_zmq_reply.h"
#include "transporter.h"
#include "futils.h"
#include "tpad_error.h"
#include "rnum.h"

typedef struct dirent dir_t;

static int regfilesonly(const dir_t *entry)
{
	// Filter to find all regular files
	if(is_regfile(entry->d_name, 0) == 1) return 1;

	// default deny
	return 0;
}

static int count_files(char *dir)
{
	int entries;
	int retval = 0;
	dir_t **farray = NULL;	// our array of directory entries

	// scans dir for all files, use filter to weed out non-files
	// then the alphasort function to sort, and put the result in g_mod_dirent
	entries = scandir(dir, &farray, regfilesonly, alphasort);
	if(entries < 0) { return -1; }

	retval = entries;

	//Loop to free all dir entries since scandir made malloc calls
	if(entries > 0) {
		while(entries--) { free(farray[entries]); }
		free(farray);
	}

	return retval;
}

static void do_count(zmq_reply_t *r)
{
	int n, filecount;
	char errmsg[128];
	char resp[24];
	char empty[4];

	filecount = count_files(".");
	if(filecount < 0) {
		snprintf(errmsg, sizeof(errmsg), "ERROR COUNTING FILES");
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	n = snprintf(resp, sizeof(resp), "%d", filecount);
	memset(empty, 0, sizeof(empty));
	(void) as_zmq_reply_send(r, TSTAT_OK,	strlen(TSTAT_OK)+1,	1);
	(void) as_zmq_reply_send(r, resp,		n+1,				1);
	(void) as_zmq_reply_send(r, empty,		1,					0);
}

// https://stackoverflow.com/questions/31633943/compare-two-times-in-c
// http://www.cplusplus.com/reference/ctime/difftime/
int oldestfirst(const struct dirent **d1, const struct dirent **d2)
{
	struct stat s1, s2;
	const char *p1, *p2;

	p1 = (*d1)->d_name;
	p2 = (*d2)->d_name;
	if(stat(p1, &s1) == 0) {
		if(stat(p2, &s2) == 0) {
			if(s1.st_mtime > s2.st_mtime) { return 1; }
			if(s2.st_mtime > s1.st_mtime) { return -1; }
		}
	}

	return 0;
}

int newestfirst(const struct dirent **d1, const struct dirent **d2)
{
	struct stat s1, s2;
	const char *p1, *p2;

	p1 = (*d1)->d_name;
	p2 = (*d2)->d_name;
	if(stat(p1, &s1) == 0) {
		if(stat(p2, &s2) == 0) {
			if(s1.st_mtime > s2.st_mtime) { return -1; }
			if(s2.st_mtime > s1.st_mtime) { return 1; }
		}
	}

	return 0;
}

int largestfirst(const struct dirent **d1, const struct dirent **d2)
{
	struct stat s1, s2;
	const char *p1, *p2;

	p1 = (*d1)->d_name;
	p2 = (*d2)->d_name;
	if(stat(p1, &s1) == 0) {
		if(stat(p2, &s2) == 0) {
			if(s1.st_size > s2.st_size) { return -1; }
			if(s2.st_size > s1.st_size) { return 1; }
		}
	}

	return 0;
}

int smallestfirst(const struct dirent **d1, const struct dirent **d2)
{
	struct stat s1, s2;
	const char *p1, *p2;

	p1 = (*d1)->d_name;
	p2 = (*d2)->d_name;
	if(stat(p1, &s1) == 0) {
		if(stat(p2, &s2) == 0) {
			if(s1.st_size > s2.st_size) { return 1; }
			if(s2.st_size > s1.st_size) { return -1; }
		}
	}

	return 0;
}

static void pick_file(zmq_reply_t *r, char *dir, int method)
{
	int entries, which, n;
	char errmsg[128];
	char resp[1024+1];
	char empty[4];
	dir_t **farray = NULL;	// our array of directory entries

	// scans dir for all files, use filter to weed out non-files
	// then the used a specified sorting function to sort
	if(method == OLDMETHOD) {
		entries = scandir(dir, &farray, regfilesonly, oldestfirst);
	} else if(method == NEWMETHOD) {
		entries = scandir(dir, &farray, regfilesonly, newestfirst);
	} else if(method == LRGMETHOD) {
		entries = scandir(dir, &farray, regfilesonly, largestfirst);
	} else if(method == SMLMETHOD) {
		entries = scandir(dir, &farray, regfilesonly, smallestfirst);
	} else {
		entries = scandir(dir, &farray, regfilesonly, alphasort);
	}

	if(entries < 0) {
		snprintf(errmsg, sizeof(errmsg), "scandir() FAILED");
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	//Loop to free all dir entries since scandir made malloc calls
	if(entries > 0) {
		which = 0;
		if(method == RNDMETHOD) { which = rpick(entries); }
		n = snprintf(resp, sizeof(resp), "%s", farray[which]->d_name);
		while(entries--) { free(farray[entries]); }
		free(farray);
	} else {
		snprintf(errmsg, sizeof(errmsg), "ZERO FILES AVAILABLE");
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	memset(empty, 0, sizeof(empty));
	(void) as_zmq_reply_send(r, TSTAT_OK,	strlen(TSTAT_OK)+1,	1);
	(void) as_zmq_reply_send(r, resp,		n+1,				1);
	(void) as_zmq_reply_send(r, empty,		1,					0);
}

void tpad_cmd(zmq_reply_t *r, zmq_mf_t *msg2, zmq_mf_t *msg3, zmq_mf_t *msg4)
{
	char *cmd;
	char errmsg[128];

	cmd = (char *)msg2->buf;
	if((strlen(cmd) < 5) || (strlen(cmd) > 24)) {
		snprintf(errmsg, sizeof(errmsg), "INVALID COMMAND");
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

#ifdef DEBUG
	printf("%s(): %s %s\n", __func__, "CMD", cmd);
#endif

	if(strcmp(cmd, COUNTCMD) == 0) {
		do_count(r);
		return;
	}

	if(strcmp(cmd, RANDOMCMD) == 0) {
		pick_file(r, ".", RNDMETHOD);
		return;
	}

	if(strcmp(cmd, OLDESTCMD) == 0) {
		pick_file(r, ".", OLDMETHOD);
		return;
	}

	if(strcmp(cmd, NEWESTCMD) == 0) {
		pick_file(r, ".", NEWMETHOD);
		return;
	}

	if(strcmp(cmd, LARGESTCMD) == 0) {
		pick_file(r, ".", LRGMETHOD);
		return;
	}

	if(strcmp(cmd, SMALLESTCMD) == 0) {
		pick_file(r, ".", SMLMETHOD);
		return;
	}

	snprintf(errmsg, sizeof(errmsg), "INVALID COMMAND");
	tpad_error(r, __func__, errmsg, NULL);
}
