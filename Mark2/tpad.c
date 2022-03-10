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
#include <signal.h>
#include <errno.h>
#include <time.h>

#include "getopts.h"
#include "async_zmq_reply.h"
#include "transporter.h"
#include "futils.h"
#include "gchelper.h"

static void parse_args(int argc, char **argv);
void tpad_cb(zmq_reply_t *r, zmq_mf_t **mpa, int msgcnt, void *user_data);

int g_shutdown = 0;
int g_noclobber = 0;

char *g_zmqaddr = NULL;
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
	zmq_reply_t *zrep = NULL;

	srand(time(NULL));
	tpad_gcinit(TPAD_GCRYPT_MINVERS);
	parse_args(argc, argv);

	z = chdir(g_outputdir);
	if(z == -1) { fprintf(stderr, "chdir(%s) failed: %s\n", g_outputdir, strerror(errno)); exit(1); }

	zrep = as_zmq_reply_create(g_zmqaddr, tpad_cb, 0, 0, 0, NULL);
	if(!zrep) {
		fprintf(stderr, "as_zmq_reply_create(%s) failed!\n", g_zmqaddr);
		return 1;
	}

	signal(SIGINT,		sig_handler);
	signal(SIGTERM, 	sig_handler);
	signal(SIGQUIT,		sig_handler);
	signal(SIGHUP,		sig_handler);
	//signal(SIGALRM,	sig_handler);
	//alarm(1);

	z=0;	// Wait for the sweet release of death
	while(!g_shutdown) { if(z>999) { z=0; } usleep(1000); z++; }

	as_zmq_reply_destroy(zrep);
	if(g_zmqaddr) free(g_zmqaddr);
	if(g_outputdir) free(g_outputdir);
	return 0;
}

struct options opts[] = 
{
	{ 1, "ZMQ",	"Set the ZMQ BIND address",			"Z", 1 },
	{ 2, "dir",	"chdir() to save files in",			"d", 1 },
	{ 3, "nc",	"Do not clobber existing files",	NULL, 0 },
	{ 0, NULL,	NULL,								NULL, 0 }
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
				g_noclobber = 1;
				break;
			default:
				fprintf(stderr, "Unexpected getopts Error! (%d)\n", c);
				break;
		}

		//This free() is required since getopts() automagically allocates space for "args" everytime it's called.
		free(args);
	}

	if(!g_zmqaddr) {
		fprintf(stderr, "I need an address for the ZMQ REPLY bus! (Fix with -Z)\n");
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
