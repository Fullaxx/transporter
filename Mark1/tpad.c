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
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>

#include "getopts.h"
#include "async_zmq_reply.h"
#include "transporter.h"
#include "futils.h"
#include "gchelper.h"

typedef struct dirent dir_t;

void parse_args(int argc, char **argv);
void tpad_put_cb(zmq_reply_t *r, zmq_mf_t **mpa, int msgcnt, void *user_data);
void tpad_get_cb(zmq_reply_t *r, zmq_mf_t **mpa, int msgcnt, void *user_data);
void tpad_del_cb(zmq_reply_t *r, zmq_mf_t **mpa, int msgcnt, void *user_data);

int g_shutdown = 0;

char *g_zPUTRepAddr = NULL;
char *g_zGETRepAddr = NULL;
char *g_zDELRepAddr = NULL;
char *g_outputdir = NULL;

void sig_handler(int signum)
{
	switch(signum) {
		case SIGALRM:
			//Do something useful here
			//alarm(1);
			break;
		case SIGHUP:
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			g_shutdown = 1;
			break;
	}
}

int main(int argc, char **argv)
{
	int z;
	zmq_reply_t *putrep = NULL;
	zmq_reply_t *getrep = NULL;
	zmq_reply_t *delrep = NULL;

	tpad_gcinit(TPAD_GCRYPT_MINVERS);
	parse_args(argc, argv);

	z = chdir(g_outputdir);
	if(z == -1) { fprintf(stderr, "chdir(%s) failed: %s\n", g_outputdir, strerror(errno)); exit(1); }

	if(g_zPUTRepAddr) {
		putrep = as_zmq_reply_create(g_zPUTRepAddr, tpad_put_cb, 0, 0, 0, NULL);
		if(!putrep) {
			fprintf(stderr, "as_zmq_reply_create(%s) failed!\n", g_zPUTRepAddr);
			return 1;
		}
	}

	if(g_zGETRepAddr) {
		getrep = as_zmq_reply_create(g_zGETRepAddr, tpad_get_cb, 0, 0, 0, NULL);
		if(!getrep) {
			fprintf(stderr, "as_zmq_reply_create(%s) failed!\n", g_zGETRepAddr);
			return 1;
		}
	}

	if(g_zDELRepAddr) {
		delrep = as_zmq_reply_create(g_zDELRepAddr, tpad_del_cb, 0, 0, 0, NULL);
		if(!delrep) {
			fprintf(stderr, "as_zmq_reply_create(%s) failed!\n", g_zDELRepAddr);
			return 1;
		}
	}

	signal(SIGINT,		sig_handler);
	signal(SIGTERM, 	sig_handler);
	signal(SIGQUIT,		sig_handler);
	signal(SIGHUP,		sig_handler);
	//signal(SIGALRM,	sig_handler);
	//alarm(1);

	// Wait for the sweet release of death
	z=0; while(!g_shutdown) { if(z>999) { z=0; } usleep(1000); z++; }

	as_zmq_reply_destroy(delrep);
	as_zmq_reply_destroy(getrep);
	as_zmq_reply_destroy(putrep);
	if(g_zDELRepAddr) free(g_zDELRepAddr);
	if(g_zGETRepAddr) free(g_zGETRepAddr);
	if(g_zPUTRepAddr) free(g_zPUTRepAddr);
	if(g_outputdir) free(g_outputdir);
	return 0;
}

struct options opts[] = 
{
	{ 1, "PUT",	"Set the ZMQ BIND address for PUT",	"P", 1 },
	{ 2, "GET",	"Set the ZMQ BIND address for GET",	"G", 1 },
	{ 3, "DEL",	"Set the ZMQ BIND address for DEL",	"D", 1 },
	{ 4, "dir",	"chdir() to save files in",			"d", 1 },
	{ 0, NULL,	NULL,								NULL, 0 }
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
				g_zPUTRepAddr = strdup(args);
				break;
			case 2:
				g_zGETRepAddr = strdup(args);
				break;
			case 3:
				g_zDELRepAddr = strdup(args);
				break;
			case 4:
				g_outputdir = strdup(args);
				break;
			default:
				fprintf(stderr, "Unexpected getopts Error! (%d)\n", c);
				break;
		}

		//This free() is required since getopts() automagically allocates space for "args" everytime it's called.
		free(args);
	}

	if(!g_zPUTRepAddr) {
		fprintf(stderr, "I need an address for the ZMQ PUT REPLY bus! (Fix with -P)\n");
		exit(EXIT_FAILURE);
	}

	if(!g_zGETRepAddr) {
		fprintf(stderr, "I need an address for the ZMQ GET REPLY bus! (Fix with -G)\n");
		exit(EXIT_FAILURE);
	}

	if(!g_zDELRepAddr) {
		fprintf(stderr, "I need an address for the ZMQ DEL REPLY bus! (Fix with -D)\n");
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
