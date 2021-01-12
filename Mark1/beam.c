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

typedef struct dirent dir_t;

void parse_args(int argc, char **argv);

char *g_zReqAddr = NULL;

char *g_file = NULL;
char *g_inputdir = NULL;

int g_recursive = 0;
int g_verbosity = 1;
int g_delete = 1;

static void print_error(void *req)
{
	int n;
	char errmsg[1024];

	memset(errmsg, 0, sizeof(errmsg));
	n = zmq_recv(req, errmsg, sizeof(errmsg)-1, 0);
	if(n == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return; }

	fprintf(stderr, "%s: %s\n", TCMD_ERR, errmsg);
}

static void send_file(void *socket, char *filename)
{
	long size;
	size_t bytes;
	int z, r;
	gcfile_t gcf;
	char *buf, *tmp;
	char *rmt_fname;
	char status[16];
	char rmt_hash[((512/8)*2)+1+1];
	char *hash = NULL;

	size = file_size(filename, 1);
	if(size < 0) return;

	// We are going to set a filesize limit of 2G since we are going to malloc the whole thing
	if(size > MAXFILESIZE) { fprintf(stderr, "file_size(%s) > %ld!\n", filename, MAXFILESIZE); return; }

	GCFILE_INIT(&gcf);
	z = gcfile_open(&gcf, filename, "r");
	if(z != 0) { fprintf(stderr, "%s\n", gcfile_geterrmsg(&gcf)); return; }

	z = gcfile_enable(&gcf, TPAD_HASH_ALG);
	if(z < 0) { fprintf(stderr, "%s\n", gcfile_geterrmsg(&gcf)); return; }

	buf = malloc(size);
	if(!buf) { gcfile_close(&gcf); fprintf(stderr, "malloc(%lu) failed!\n", size); return; }

	bytes = gcfile_read(&gcf, buf, size);
	if(bytes != size) { gcfile_close(&gcf); free(buf); fprintf(stderr, "%s\n", gcfile_geterrmsg(&gcf)); return; }

	if(g_verbosity >= 1) { printf("Beaming File: %s(%ld) ... ", filename, size); }

	tmp = strdup(filename);
	rmt_fname = basename(tmp);
	(void) zmq_send(socket, TCMD_PUT,	strlen(TCMD_PUT)+1,		ZMQ_SNDMORE);
	(void) zmq_send(socket, rmt_fname,	strlen(rmt_fname)+1,	ZMQ_SNDMORE);
	(void) zmq_send(socket, buf,		size,					0);
	free(tmp);
	free(buf);

	memset(status, 0, sizeof(status));
	r = zmq_recv(socket, status, sizeof(status)-1, 0);
	if(r == -1) { fprintf(stderr, "zmq_recv() failed: %s\n", strerror(errno)); }

	if(strcmp(status, TCMD_ERR) == 0) { print_error(socket); return; }

	if(strcmp(status, TCMD_OK) != 0) {
		// Something went horribly wrong!
		fprintf(stderr, "UNKNOWN STATUS: %s\n", status);
		exit(99);
	}

	// Recv the hash that the server genereated
	memset(rmt_hash, 0, sizeof(rmt_hash));
	r = zmq_recv(socket, rmt_hash, sizeof(rmt_hash)-1, 0);
	if(r == -1) { fprintf(stderr, "zmq_recv() failed: %s\n", strerror(errno)); }
	else { hash = gcfile_get_hash(&gcf, TPAD_HASH_ALG); }

	// Check remote hash against local hash
	if(hash) {
		if(strcmp(hash, rmt_hash) == 0) {
			if(g_delete) { remove(filename); }
			if(g_verbosity >= 1) { printf("%s\n", status); }
		} else {
			if(g_verbosity >= 1) { printf("%s\n", "HASH ERROR"); }
		}
		free(hash);
	}
	gcfile_close(&gcf);
	GCFILE_INIT(&gcf);
}

static int filter(const dir_t *entry)
{
	if(is_regfile(entry->d_name, 0) == 1) return 1;

	return 0;	/*	default	(Bad)*/
}

static void send_dir(void *socket, char *dir)
{
	int i, z, entries;
	dir_t **mod_dirent = NULL;

	z = chdir(dir);
	if(z == -1) { fprintf(stderr, "chdir(%s) failed: %s\n", dir, strerror(errno)); return; }

	// scans dir for all files, use filter to weed out non-files
	// then the alpha sort function to sort, and put the result in g_mod_dirent
	entries = scandir(".", &mod_dirent, filter, alphasort);

	// Do something with each file
	for (i=0; i<entries; i++) {
		send_file(socket, mod_dirent[i]->d_name);
	}

	// Loop to free all dir entries since scandir made malloc calls
	if(entries > 0) {
		while(entries--) {
			free(mod_dirent[entries]);
		}
		free(mod_dirent);
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
	zmq_connect(zReqSock, g_zReqAddr);

	if(g_file) send_file(zReqSock, g_file);
	if(g_inputdir) send_dir(zReqSock, g_inputdir);

	zmq_close(zReqSock);
	zmq_ctx_destroy(zContext);

	if(g_file) free(g_file);
	if(g_inputdir) free(g_inputdir);
	return 0;
}

struct options opts[] = 
{
	{ 1, "PUT",		"Connect to this TPAD ZMQ PUT SOCK",		"P",  1 },
	{ 2, "file",	"Beam this file to the TPAD",				"f",  1 },
	{ 3, "dir",		"Beam all files in this dir to the TPAD",	"d",  1 },
	{ 4, "quiet",	"Be less verbose",							"q",  0 },
	{ 5, "keep",	"Do not delete files after transmission",	NULL, 0 },
//	{ 6, "recurse",	"recursive",								"r",  0 },
	{ 0, NULL,		NULL,										NULL, 0 }
};

void parse_args(int argc, char **argv)
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
				g_zReqAddr = strdup(args);
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
			/*case 6:
				g_recursive = 1;
				break;*/
			default:
				fprintf(stderr, "Unexpected getopts Error! (%d)\n", c);
				break;
		}

		//This free() is required since getopts() automagically allocates space for "args" everytime it's called.
		free(args);
	}

	if(!g_zReqAddr) {
		fprintf(stderr, "I need an address to connect to! (Fix with -P)\n");
		exit(EXIT_FAILURE);
	}

	/*if(!g_inputdir) {
		fprintf(stderr, "I need a dir to save files to! (Fix with -d)\n");
		exit(EXIT_FAILURE);
	}*/

}
