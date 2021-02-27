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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <zmq.h>

#include "getopts.h"
#include "transporter.h"
#include "futils.h"
#include "gchelper.h"
#include "gcryptfile.h"

void parse_args(int argc, char **argv);

char *g_zGETAddr = NULL;
char *g_zDELAddr = NULL;
void *g_zContext, *g_zGETReq, *g_zDELReq;

char *g_outputdir = NULL;
int g_verbosity = 1;

static void print_error(void *req)
{
	int n;
	char errmsg[1024];

	memset(errmsg, 0, sizeof(errmsg));
	n = zmq_recv(req, errmsg, sizeof(errmsg)-1, 0);
	if(n == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return; }

	fprintf(stderr, "%s: %s\n", TCMD_ERR, errmsg);
}

static int del_remote_file(char *filename, size_t size, char *filehash)
{
	int z;
	char filesize[64];
	char status[16];

	snprintf(filesize, sizeof(filesize), "%lu", size);
	zmq_send(g_zDELReq, TCMD_DEL,	strlen(TCMD_DEL)+1,	ZMQ_SNDMORE);
	zmq_send(g_zDELReq, filename,	strlen(filename)+1,	ZMQ_SNDMORE);
	zmq_send(g_zDELReq, filesize,	strlen(filesize)+1,	ZMQ_SNDMORE);
	zmq_send(g_zDELReq, filehash,	strlen(filehash)+1,	0);

	// Receive Status
	memset(status, 0, sizeof(status));
	z = zmq_recv(g_zDELReq, status, sizeof(status)-1, 0);
	if(z == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return -1; }

	if(strcmp(status, TCMD_ERR) == 0) { print_error(g_zDELReq); return -2; }

	if(g_verbosity >= 1) printf("%s\n", status);
	return 0;
}

static int absorb_file(char *freq)
{
	int z;
	size_t r;
	char status[16];
	char recvsize[24];
	char filename[1024+1];
	char filesize[24];
	long size;
	char *contents = NULL;
	gcfile_t gcf;
	char *hash;

	memset(filename, 0, sizeof(filename));

	snprintf(recvsize, sizeof(recvsize), "%ld", MAXFILESIZE);
	zmq_send(g_zGETReq, TCMD_GET,	strlen(TCMD_GET)+1,	ZMQ_SNDMORE);
	zmq_send(g_zGETReq, freq,		strlen(freq)+1,		ZMQ_SNDMORE);
	zmq_send(g_zGETReq, recvsize,	strlen(recvsize)+1,	0);

	// Receive Status
	memset(status, 0, sizeof(status));
	z = zmq_recv(g_zGETReq, status, sizeof(status)-1, 0);
	if(z == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return -1; }

	if(strcmp(status, TCMD_ERR) == 0) { print_error(g_zGETReq); return -2; }

	// Receive File Name
	memset(filename, 0, sizeof(filename));
	z = zmq_recv(g_zGETReq, filename, sizeof(filename)-1, 0);
	if(z == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return -3; }

	// Receive File Size
	memset(filesize, 0, sizeof(filesize));
	z = zmq_recv(g_zGETReq, filesize, sizeof(filesize)-1, 0);
	if(z == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return -4; }

	// Receive File Content
	size = atol(filesize);
	contents = malloc(size);
	z = zmq_recv(g_zGETReq, contents, size, 0);
	if(z == -1) { free(contents); fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return -5; }

	GCFILE_INIT(&gcf);
	z = gcfile_open(&gcf, filename, "w");
	if(z != 0) { free(contents); fprintf(stderr, "%s\n", GCFILE_GETERRMSG(&gcf)); return -6; }

	z = gcfile_enable(&gcf, TPAD_HASH_ALG);
	if(z < 0) { free(contents); fprintf(stderr, "%s\n", GCFILE_GETERRMSG(&gcf)); return -7; }

	r = gcfile_write(&gcf, contents, size);
	free(contents);
	if(r != size) { gcfile_close(&gcf); fprintf(stderr, "%s\n", GCFILE_GETERRMSG(&gcf)); return -8; }

	hash = gcfile_get_hash(&gcf, TPAD_HASH_ALG);
	if(g_verbosity >= 1) printf("Absorbing File: %s(%lu) ... ", filename, r);
	z = del_remote_file(filename, r, hash);

	free(hash);
	gcfile_close(&gcf);
	GCFILE_INIT(&gcf);
	return z;
}

static long req_count(void)
{
	int n;
	char status[16];
	char resp[128];
	long count;

	memset(resp, 0, sizeof(resp));
	zmq_send(g_zGETReq, TCMD_GET,	strlen(TCMD_GET)+1,	ZMQ_SNDMORE);
	zmq_send(g_zGETReq, COUNTCMD,	strlen(COUNTCMD)+1,	ZMQ_SNDMORE);
	zmq_send(g_zGETReq, resp,		1,					0);

	// Receive Status
	memset(status, 0, sizeof(status));
	n = zmq_recv(g_zGETReq, status, sizeof(status)-1, 0);
	if(n == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return -1; }

	if(strcmp(status, TCMD_ERR) == 0) { print_error(g_zGETReq); return -2; }

	// Receive File Count
	memset(resp, 0, sizeof(resp));
	n = zmq_recv(g_zGETReq, resp, sizeof(resp)-1, 0);
	if(n == -1) { fprintf(stderr, "%s(): zmq_recv(): %s\n", __func__, strerror(errno)); return -3; }

	count = atol(resp);
	if(g_verbosity >= 2) printf("files available: %lu\n", count);
	return count;
}

int main(int argc, char *argv[])
{
	int z;
	long remote_file_count;

	tpad_gcinit(TPAD_GCRYPT_MINVERS);
	parse_args(argc, argv);

	z = chdir(g_outputdir);
	if(z == -1) { fprintf(stderr, "chdir(%s) failed: %s\n", g_outputdir, strerror(errno)); exit(1); }

	g_zContext = zmq_ctx_new();
	g_zGETReq = zmq_socket(g_zContext, ZMQ_REQ);
	zmq_connect(g_zGETReq, g_zGETAddr);
	g_zDELReq = zmq_socket(g_zContext, ZMQ_REQ);
	zmq_connect(g_zDELReq, g_zDELAddr);

	remote_file_count = req_count();
	while(remote_file_count > 0) {
		z = absorb_file(RANDOMCMD);
		if(z != 0) { break; }
		//sleep(1);
		remote_file_count = req_count();
	}

	zmq_close(g_zDELReq);
	zmq_close(g_zGETReq);
	zmq_ctx_destroy(g_zContext);
	if(g_zGETAddr) free(g_zGETAddr);
	if(g_zDELAddr) free(g_zDELAddr);
	return 0;
}

struct options opts[] = 
{
	{ 1, "GET",		"Connect to this TPAD ZMQ GET SOCK",	"G", 1 },
	{ 2, "DEL",		"Connect to this TPAD ZMQ DEL SOCK",	"D", 1 },
	{ 3, "dir",		"Choose a dir to save files to",		"d", 1 },
	{ 4, "quiet",	"Be less verbose",						"q", 0 },
	{ 0, NULL,		NULL,									NULL, 0 }
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
				g_zGETAddr = strdup(args);
				break;
			case 2:
				g_zDELAddr = strdup(args);
				break;
			case 3:
				g_outputdir = strdup(args);
				break;
			case 4:
				g_verbosity--;
				break;
			default:
				fprintf(stderr, "Unexpected getopts Error! (%d)\n", c);
				break;
		}

		//This free() is required since getopts() automagically allocates space for "args" everytime it's called.
		free(args);
	}

	if(!g_zGETAddr) {
		fprintf(stderr, "I need an address to connect to! (Fix with -G)\n");
		exit(EXIT_FAILURE);
	}

	if(!g_zDELAddr) {
		fprintf(stderr, "I need an address to connect to! (Fix with -D)\n");
		exit(EXIT_FAILURE);
	}

	if(!g_outputdir) {
		fprintf(stderr, "I need a dir to save files to! (Fix with -d)\n");
		exit(EXIT_FAILURE);
	}

	if(!is_dir(g_outputdir, 1)) {
		//fprintf(stderr, "%s is not a directory!\n", g_outputdir);
		exit(EXIT_FAILURE);
	}
}
