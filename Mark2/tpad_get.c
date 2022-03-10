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
//#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "async_zmq_reply.h"
#include "transporter.h"
#include "futils.h"
#include "tpad_error.h"
#include "gchelper.h"
#include "xfer.h"

typedef struct dirent dir_t;

/*	absorb.c
	z = zmq_send(s, TCMD_GET,	strlen(TCMD_GET)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, freq,		strlen(freq)+1,		ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,					ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,					0);
*/
static void get_file(zmq_reply_t *r, char *filename)
{
	int z;
	long size;
	xfer_t *xp;
	char filesize[24];
	char errmsg[1536];

/*
#ifdef DEBUG
	printf("%s(): %s %s\n", __func__, "GET", filename);
#endif
*/

	if(file_security_check(filename)) {
		snprintf(errmsg, sizeof(errmsg), "INVALID FILENAME %s", filename);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	size = file_size(filename, 0);
	if(size <= 0) {
		snprintf(errmsg, sizeof(errmsg), "BAD FILESIZE: %ld(%s)", size, filename);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	// Find an open transfer slot, then
	// Make a new entry with filename, filesize, uuid, current offset
	xp = xfer_new(filename, "r");
	if(!xp) {
		snprintf(errmsg, sizeof(errmsg), "Could not get open xfer slot for %s", filename);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

#ifdef DEBUG
	printf("%s(): %s %s(%ld/%s)\n", __func__, "XFER", filename, size, xp->uuid);
#endif

	z = gcfile_enable(&xp->gcf, TPAD_HASH_ALG);
	if(z != 0) {
		xfer_complete(xp, 0);
		snprintf(errmsg, sizeof(errmsg), "gcfile_enable(%d) failed: %s", TPAD_HASH_ALG, GCFILE_GETERRMSG(&xp->gcf));
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	// Fill in the details
	xp->size = size;
	xp->offset = 0L;

	snprintf(filesize, sizeof(filesize), "%ld", size);
	(void) as_zmq_reply_send(r, TSTAT_OK,	strlen(TSTAT_OK)+1,	1);
	(void) as_zmq_reply_send(r, filename,	strlen(filename)+1,	1);
	(void) as_zmq_reply_send(r, filesize,	strlen(filesize)+1,	1);
	(void) as_zmq_reply_send(r, xp->uuid,	strlen(xp->uuid)+1,	0);
}

void tpad_get(zmq_reply_t *r, zmq_mf_t *msg2, zmq_mf_t *msg3, zmq_mf_t *msg4)
{
	char *filename = NULL;
	char errmsg[128];

	filename = (char *)msg2->buf;
	if((strlen(filename) < 1) || (strlen(filename) > 1024)) {
		snprintf(errmsg, sizeof(errmsg), "INVALID FILENAME");
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

#ifdef DEBUG
	printf("%s(): %s %s\n", __func__, "GET", filename);
#endif

	get_file(r, filename);
}
