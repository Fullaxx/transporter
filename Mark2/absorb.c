/*
	Transporter is a ZMQ based file transfer utility
	Copyright (C) 2022 Brett Kuskie <fullaxx@gmail.com>

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
#include "stats.h"

static void parse_args(int argc, char **argv);

char *g_zmqaddr = NULL;
void *g_zContext;

char *g_outputdir = NULL;
int g_verbosity = 1;

#ifdef ABSORB_NOCLOBBER
int g_noclobber = 0;
#endif

char g_uuid[64+1];
long g_BS = 1000;
char *g_method = RANDOMCMD;

static int check_hash(void *s, gcfile_t *gcf, long bytes)
{
	int z;
	char *hash, *stats;
	char offset[32];
	char status[16];
	char msg2[1536];
	char msg3[256];

	memset(offset,	0, sizeof(offset));
	memset(status,	0, sizeof(status));
	memset(msg2,	0, sizeof(msg2));
	memset(msg3,	0, sizeof(msg3));

	hash = gcfile_get_hash(gcf, TPAD_HASH_ALG);

	snprintf(offset, sizeof(offset), "%ld", bytes);
	z = zmq_send(s, TCMD_XFR,	strlen(TCMD_XFR)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, g_uuid,		sizeof(g_uuid),		ZMQ_SNDMORE);
	z = zmq_send(s, offset,		sizeof(offset),		ZMQ_SNDMORE);
	z = zmq_send(s, hash,		strlen(hash)+1,		0);

	z = zmq_recv(s, status,	sizeof(status),	0);
	// IF TSTAT_ERR - can we do a FUNC CALL here?
	z = zmq_recv(s, msg2,	sizeof(msg2),	0);
	z = zmq_recv(s, msg3,	sizeof(msg3),	0);

	if(strcmp(status, TSTAT_ERR) == 0) {
		if(g_verbosity >= 1) { printf("ERR\n"); }
		fprintf(stderr, "%s\n", msg2);
		return -1;
	}

	stats = get_stats(gcf);
	if(g_verbosity >= 1) { printf("%s %s\n", status, stats); }
	free(stats);
	free(hash);
	return 0;
}

static long absorb_chunk(void *s, gcfile_t *gcf, long bytes)
{
	int z;
	size_t written;
	char status[16];
	char offset[32];
	char blocksize[32];
	unsigned char data[g_BS];
	char len[32];

	snprintf(offset, sizeof(offset), "%ld", bytes);
	snprintf(blocksize, sizeof(blocksize), "%ld", g_BS);
	z = zmq_send(s, TCMD_XFR,	strlen(TCMD_XFR)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, g_uuid,		sizeof(g_uuid),		ZMQ_SNDMORE);
	z = zmq_send(s, offset,		sizeof(offset),		ZMQ_SNDMORE);
	z = zmq_send(s, blocksize,	sizeof(blocksize),	0);

	if(g_verbosity >= 2) printf("Requesting Chunk: %s(%s) ...", offset, blocksize);

	z = zmq_recv(s, status,	sizeof(status),	0);
	// IF TSTAT_ERR - can we do a FUNC CALL here?
	z = zmq_recv(s, data,	sizeof(data),	0);
	z = zmq_recv(s, len,	sizeof(len),	0);

	if(g_verbosity >= 2) printf(" got %s bytes\n", len);

	if(strcmp(status, TSTAT_ERR) == 0) {
		if(g_verbosity >= 1) { printf("ERR\n"); }
		fprintf(stderr, "%s\n", data);
		return -1;
	}

	bytes = atol(len);
	if(bytes == 0) {
		if(g_verbosity >= 2) { printf("ERR\n"); }
		fprintf(stderr, "BYTES == 0\n");
		return -2;
	}

	written = gcfile_write(gcf, data, bytes);
	if(written != bytes) {
		fprintf(stderr, "written(%ld) != bytes(%ld)", written, bytes);
		return -3;
	}

	return written;
}

static int absorb_file(void *s, char *freq)
{
	int z;
	char status[16];
	char empty[4];
	char filename[1024+1];
	char filesize[32];
	long size, chunk_size, bytes;
	gcfile_t gcf;
	int err;

	memset(empty, 0, sizeof(empty));
	z = zmq_send(s, TCMD_GET,	strlen(TCMD_GET)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, freq,		strlen(freq)+1,		ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,					ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,					0);

	z = zmq_recv(s, status,		sizeof(status),		0);
	// IF TSTAT_ERR - can we do a FUNC CALL here?
	z = zmq_recv(s, filename,	sizeof(filename),	0);
	z = zmq_recv(s, filesize,	sizeof(filesize),	0);
	z = zmq_recv(s, g_uuid,		sizeof(g_uuid),		0);

	//if(g_verbosity >= 1) printf("Absorbing File: %s(%s) ... ", filename, filesize);
	if(g_verbosity >= 1) printf("Absorbing File: %s ... ", filename);

	if(strcmp(status, TSTAT_ERR) == 0) {
		if(g_verbosity >= 1) { printf("ERR\n"); }
		fprintf(stderr, "%s\n", filename);
		return -1;
	}

#ifdef ABSORB_NOCLOBBER
	// If we set no_clobber, error if file exists
	if(g_noclobber) {
		int file_exists = is_regfile(filename, 0);
		if(file_exists != -1) {
			if(g_verbosity >= 1) printf("EXISTS\n");
			if(g_verbosity >= 2) printf("%s exists and we are set to no_clobber!\n", filename);
			// If we do this, we have to tell the server that we are aborting the transfer, so it can free the xfer slot
			return -2;
		}
	}
#endif

	if(g_verbosity >= 2) { printf("\n"); }

	GCFILE_INIT(&gcf);
	z = gcfile_open(&gcf, filename, "w");
	if(z != 0) { fprintf(stderr, "%s\n", GCFILE_GETERRMSG(&gcf)); return -3; }

	z = gcfile_enable(&gcf, TPAD_HASH_ALG);
	if(z < 0) { fprintf(stderr, "%s\n", GCFILE_GETERRMSG(&gcf)); return -4; }

	size = atol(filesize);
	bytes = 0;

	while(bytes < size) {
		chunk_size = absorb_chunk(s, &gcf, bytes);
		if(chunk_size <= 0) { return -5; }
		bytes += chunk_size;
	}

	// Once the transfer it complete, check the hash with the server
	err = check_hash(s, &gcf, bytes);

	gcfile_close(&gcf);
	GCFILE_INIT(&gcf);
	return err;
}

// This must be free()'d
static char* req_file(void *s, char *method)
{
	int z;
	char empty[4];
	char status[16];
	char resp[1024+1];

	memset(empty, 0, sizeof(empty));
	z = zmq_send(s, TCMD_CMD,	strlen(TCMD_CMD)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, method,		strlen(method)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,					ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,					0);

	// Receive Status and File Count
	memset(status, 0, sizeof(status));
	memset(resp, 0, sizeof(resp));
	z = zmq_recv(s, status,	sizeof(status)-1,	0);
	z = zmq_recv(s, resp,	sizeof(resp)-1,		0);
	z = zmq_recv(s, empty,	sizeof(empty)-1,	0);

	//if(strcmp(status, TSTAT_ERR) == 0) { print_error(s); return -1; }

	if(strcmp(status, TSTAT_ERR) == 0) {
		if(g_verbosity >= 1) { printf("ERR\n"); }
		fprintf(stderr, "%s\n", resp);
		return NULL;
	}

	if(g_verbosity >= 2) printf("file: %s\n", resp);
	return strdup(resp);
}

static long req_count(void *s)
{
	int z;
	char empty[4];
	char status[16];
	char resp[64];
	long count = 0;

	memset(empty, 0, sizeof(empty));
	z = zmq_send(s, TCMD_CMD,	strlen(TCMD_CMD)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, COUNTCMD,	strlen(COUNTCMD)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,					ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,					0);

	// Receive Status and File Count
	memset(status, 0, sizeof(status));
	memset(resp, 0, sizeof(resp));
	z = zmq_recv(s, status,	sizeof(status)-1,	0);
	z = zmq_recv(s, resp,	sizeof(resp)-1,		0);
	z = zmq_recv(s, empty,	sizeof(empty)-1,	0);

	//if(strcmp(status, TSTAT_ERR) == 0) { print_error(s); return -1; }

	if(strcmp(status, TSTAT_ERR) == 0) {
		if(g_verbosity >= 1) { printf("ERR\n"); }
		fprintf(stderr, "%s\n", resp);
		return -1;
	}

	count = atol(resp);
	if(g_verbosity >= 2) printf("files available: %lu\n", count);
	return count;
}

int main(int argc, char *argv[])
{
	int z;
	void *zSock;
	char *incfile;
	long remote_file_count;

	tpad_gcinit(TPAD_GCRYPT_MINVERS);
	parse_args(argc, argv);

	z = chdir(g_outputdir);
	if(z == -1) { fprintf(stderr, "chdir(%s) failed: %s\n", g_outputdir, strerror(errno)); exit(1); }

	g_zContext = zmq_ctx_new();
	zSock = zmq_socket(g_zContext, ZMQ_REQ);
	zmq_connect(zSock, g_zmqaddr);

	remote_file_count = req_count(zSock);
	while(remote_file_count > 0) {
		incfile = req_file(zSock, g_method);
		if(incfile) {
			z = absorb_file(zSock, incfile);
			free(incfile);
		}
		if(z != 0) { break; }
		//sleep(1);
		remote_file_count = req_count(zSock);
	}

	zmq_close(zSock);
	zmq_ctx_destroy(g_zContext);
	if(g_zmqaddr) free(g_zmqaddr);
	return 0;
}

struct options opts[] = 
{
	{  1, "ZMQ",		"Connect to this TPAD ZMQ SOCK",	"Z", 1 },
	{  2, "dir",		"Choose a dir to save files to",	"d", 1 },
	{  3, "quiet",		"Be less verbose",					"q", 0 },
	{  4, "verbose",	"Be more verbose",					"v", 0 },
	{  5, "oldest",		"Oldest files first",				NULL, 0 },
	{  6, "newest",		"Newest files first",				NULL, 0 },
	{  7, "largest",	"Largest files first",				NULL, 0 },
	{  8, "smallest",	"Smallest files first",				NULL, 0 },
	{  9, "BS",			"Set the chunk transfer size",		NULL, 1 },
#ifdef ABSORB_NOCLOBBER
	{ 10, "nc",			"Do not clobber existing files",	NULL, 0 },
#endif
	{ 0, NULL,		NULL,									NULL, 0 }
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
				g_outputdir = strdup(args);
				break;
			case 3:
				g_verbosity--;
				break;
			case 4:
				g_verbosity++;
				break;
			case 5:
				g_method = OLDESTCMD;
				break;
			case 6:
				g_method = NEWESTCMD;
				break;
			case 7:
				g_method = LARGESTCMD;
				break;
			case 8:
				g_method = SMALLESTCMD;
				break;
			case 9:
				g_BS = atol(args);
				break;
#ifdef ABSORB_NOCLOBBER
			case 10:
				g_noclobber = 1;
				break;
#endif
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

	if(!g_outputdir) {
		fprintf(stderr, "I need a dir to save files to! (Fix with -d)\n");
		exit(EXIT_FAILURE);
	}

	if(!is_dir(g_outputdir, 1)) {
		//fprintf(stderr, "%s is not a directory!\n", g_outputdir);
		exit(EXIT_FAILURE);
	}
}
