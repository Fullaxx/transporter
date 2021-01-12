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

#ifdef USE_GNU_BASENAME
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_POSIX_BASENAME
#include <libgen.h>
#endif

#include <unistd.h>
#include <dirent.h>
#include <zmq.h>

#include "getopts.h"
#include "transporter.h"
#include "futils.h"
#include "gchelper.h"
#include "gcryptfile.h"
#include "stats.h"

typedef struct dirent dir_t;

static void parse_args(int argc, char **argv);

char *g_zmqaddr = NULL;

char *g_file = NULL;
char *g_inputdir = NULL;

int g_recursive = 0;
int g_verbosity = 1;
int g_delete = 1;

char g_uuid[64+1];
long g_BS = 1000;

/*
static void print_error(void *req)
{
	int n;
	char errmsg[1024];

	memset(errmsg, 0, sizeof(errmsg));
	n = zmq_recv(req, errmsg, sizeof(errmsg)-1, 0);
	if(n == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return; }

	fprintf(stderr, "%s: %s\n", TCMD_ERR, errmsg);
}
*/

static int send_chunk(void *s, gcfile_t *gcf, unsigned char *buf, size_t bytes)
{
	int z, r=0;
	char empty[4];
	char status[16];
	char completion[256];
	char rmt_hash[TPAD_HASH_SIZE+1];
	char *hashptr, *stats;

	memset(empty,		0, sizeof(empty));
	memset(status,		0, sizeof(status));
	memset(completion,	0, sizeof(completion));

	z = zmq_send(s, TCMD_PUT,	strlen(TCMD_PUT)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, g_uuid,		strlen(g_uuid)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, buf,		bytes,				ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,	0);

	z = zmq_recv(s, status,		sizeof(status),		0);
	z = zmq_recv(s, completion,	sizeof(completion),	0);
	z = zmq_recv(s, rmt_hash,	sizeof(rmt_hash),	0);

	// If the remote end sent us a hash
	// Check for completion
	if(strlen(rmt_hash) > 0) {
		hashptr = gcfile_get_hash(gcf, TPAD_HASH_ALG);
		stats = get_stats(gcf);
		if(strcmp(hashptr, rmt_hash) == 0) {
			if(g_verbosity >= 1) { printf("%s %s\n", status, stats); }
		} else {
			if(g_verbosity >= 1) { printf("%s\n", "HASH ERROR"); r=1; }
		}
		free(stats);
		free(hashptr);
	}

/*
#ifdef DEBUG
	printf("%s/%s/%s/%s\n", status, g_uuid, completion, rmt_hash);
#endif
*/

	return r;
}

static int send_header(void *s, char *path, long size)
{
	int z;
	char *tmp, *filename;
	char filesize[64];
	char status[16];
	char msg2[1536];
	char msg3[256];

	memset(filesize,	0, sizeof(filesize));
	memset(status,		0, sizeof(status));
	memset(g_uuid,		0, sizeof(g_uuid));
	memset(msg2,		0, sizeof(msg2));
	memset(msg3,		0, sizeof(msg3));

	tmp = strdup(path);
	filename = basename(tmp);
	snprintf(filesize, sizeof(filesize), "%ld", size);
	z = zmq_send(s, TCMD_PUT,	strlen(TCMD_PUT)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, g_uuid,		strlen(g_uuid)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, filename,	strlen(filename)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, filesize,	strlen(filesize)+1,	0);
	free(tmp);

	z = zmq_recv(s, status,	sizeof(status),	0);
	// IF TSTAT_ERR - can we do a FUNC CALL here?
	z = zmq_recv(s, msg2,	sizeof(msg2),	0);
	z = zmq_recv(s, msg3,	sizeof(msg3),	0);

	if(strcmp(status, TSTAT_ERR) == 0) {
		if(g_verbosity >= 1) { printf("ERR\n"); }
		fprintf(stderr, "%s\n", msg2);
		return 1;
	}

	z = sizeof(g_uuid);	//65
	memcpy(g_uuid, msg2, z-1);
	g_uuid[z-1] = 0;
	//snprintf(g_uuid, sizeof(g_uuid), "%s", msg2);

/*
#ifdef DEBUG
	printf("%s/%s/%s ", status, uuid, completion);
#endif
*/

	return 0;
}

static void send_file(void *s, char *path)
{
	int z;
	long len;
	size_t bytes;
	unsigned char buf[g_BS];
	gcfile_t gcf;

	len = file_size(path, 1);
	if(len < 0) { return; }

	GCFILE_INIT(&gcf);
	z = gcfile_open(&gcf, path, "r");
	if(z != 0) { fprintf(stderr, "%s\n", GCFILE_GETERRMSG(&gcf)); return; }

	z = gcfile_enable(&gcf, TPAD_HASH_ALG);
	if(z != 0) { fprintf(stderr, "%s\n", GCFILE_GETERRMSG(&gcf)); return; }

	//if(g_verbosity >= 1) { printf("Beaming File: %s(%ld) ... ", path, len); }
	if(g_verbosity >= 1) { printf("Beaming File: %s ... ", path); }
	z = send_header(s, path, len);
	if(z) { gcfile_close(&gcf); return; }

	while(len > 0) {
		if(len < g_BS) {
			bytes = gcfile_read(&gcf, buf, len);
		} else {
			bytes = gcfile_read(&gcf, buf, g_BS);
		}
		if(bytes == 0) { gcfile_close(&gcf); fprintf(stderr, "%s\n", GCFILE_GETERRMSG(&gcf)); return; }
		z = send_chunk(s, &gcf, buf, bytes);
		len -= bytes;
	}

	// HOW DO WE CONVEY SUCCESS FOR DELETEION
	if(g_delete && (z == 0)) { remove(path); }
	gcfile_close(&gcf);
	GCFILE_INIT(&gcf);
}

static int regfilesonly(const dir_t *entry)
{
	// Filter to find all regular files
	if(is_regfile(entry->d_name, 0) == 1) return 1;

	// default deny
	return 0;
}

static void send_dir(void *socket, char *dir)
{
	int i, z, entries;
	dir_t **farray = NULL;	// our array of directory entries

	z = chdir(dir);
	if(z == -1) { fprintf(stderr, "chdir(%s) failed: %s\n", dir, strerror(errno)); return; }

	// scans dir for all files, use filter to weed out non-files
	// then the alphasort function to sort, and put the result in g_mod_dirent
	entries = scandir(".", &farray, regfilesonly, alphasort);

	// Do something with each file
	for (i=0; i<entries; i++) {
		send_file(socket, farray[i]->d_name);
	}

	// Loop to free all dir entries since scandir made malloc calls
	if(entries > 0) {
		while(entries--) {
			free(farray[entries]);
		}
		free(farray);
	}
}

int main(int argc, char *argv[])
{
	void *zContext;
	void *zReqSock;

	tpad_gcinit(TPAD_GCRYPT_MINVERS);
	parse_args(argc, argv);

	zContext = zmq_ctx_new();
	zReqSock = zmq_socket(zContext, ZMQ_REQ);
	zmq_connect(zReqSock, g_zmqaddr);

	if(g_file) { send_file(zReqSock, g_file); }
	if(g_inputdir) { send_dir(zReqSock, g_inputdir); }

	zmq_close(zReqSock);
	zmq_ctx_destroy(zContext);

	if(g_file) free(g_file);
	if(g_inputdir) free(g_inputdir);
	return 0;
}

struct options opts[] = 
{
	{ 1, "ZMQ",		"Connect to this TPAD ZMQ SOCK",			"Z",  1 },
	{ 2, "file",	"Beam this file to the TPAD",				"f",  1 },
	{ 3, "dir",		"Beam all files in this dir to the TPAD",	"d",  1 },
	{ 4, "quiet",	"Be less verbose",							"q",  0 },
	{ 5, "keep",	"Do not delete files after transmission",	NULL, 0 },
	{ 9, "BS",		"Set the chunk transfer size",				NULL, 1 },
	{ 0, NULL,		NULL,										NULL, 0 }
};

static void parse_args(int argc, char **argv)
{
	char *args;
	int c;

	while ((c = getopts(argc, argv, opts, &args)) != 0) {
		switch(c) {
			case -2:
				// Special Case: Recognize options that we didn't set above.
				fprintf(stderr, "Unknown Getopts Option: %s\n", args);
				break;
			case -1:
				// Special Case: getopts() can't allocate memory.
				fprintf(stderr, "Unable to allocate memory for getopts().\n");
				exit(EXIT_FAILURE);
				break;
			case 1:
				g_zmqaddr = strdup(args);
				break;
			case 2:
				g_file = strdup(args);
				break;
			case 3:
				g_inputdir = strdup(args);
				break;
			case 4:
				g_verbosity--;
				break;
			case 5:
				g_delete = 0;
				break;
			case 9:
				g_BS = atol(args);
				break;
			default:
				fprintf(stderr, "Unexpected getopts Error! (%d)\n", c);
				break;
		}

		//This free() is required since getopts() automagically allocates space for "args" everytime it's called.
		free(args);
	}

	if(!g_zmqaddr) {
		fprintf(stderr, "I need an address to connect to! (Fix with -Z)\n");
		exit(EXIT_FAILURE);
	}

	/*if(!g_inputdir) {
		fprintf(stderr, "I need a dir to save files to! (Fix with -d)\n");
		exit(EXIT_FAILURE);
	}*/

}
