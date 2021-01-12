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
//#include <unistd.h>
//#include <dirent.h>
//#include <errno.h>

#include "async_zmq_reply.h"
#include "transporter.h"
#include "futils.h"
#include "tpad_error.h"
#include "gchelper.h"
#include "xfer.h"

static void get_xfer_block(zmq_reply_t *r, char *uuid, long offset, char *bshash)
{
	xfer_t *xp;
	int n, delete;
	char empty[4];
	char errmsg[128];
	long left, bytes, BS;
	unsigned char *buf;
	char len[32];
	char *hashptr;

	memset(empty, 0, sizeof(empty));

	// FIND UUID
	xp = xfer_find_uuid(uuid);
	if(!xp) {
		snprintf(errmsg, sizeof(errmsg), "UNKNOWN UUID: %s", uuid);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

#ifdef DEBUG
	printf("%s(): %s %s(%lu)\n", __func__, "XFR", GCFILE_GETPATH(&xp->gcf), offset);
#endif

	if(offset != xp->offset) {
		snprintf(errmsg, sizeof(errmsg), "BAD OFFSET: %ld != %ld", offset, xp->offset);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	if(offset == xp->size) {
		hashptr = gcfile_get_hash(&xp->gcf, TPAD_HASH_ALG);
		if(strcmp(hashptr, bshash)) {
			delete = 0;
			tpad_error(r, __func__, "INVALID HASH", NULL);
		} else {
			delete = 1;
			(void) as_zmq_reply_send(r, TSTAT_OK,	strlen(TSTAT_OK)+1,	1);
			(void) as_zmq_reply_send(r, empty,		1,					1);
			(void) as_zmq_reply_send(r, empty,		1,					0);
#ifdef DEBUG
			printf("%s(): %s %s\n", __func__, "DEL", GCFILE_GETPATH(&xp->gcf));
#endif
		}
		free(hashptr);
		xfer_complete(xp, delete);
		return;
	}

	BS = atol(bshash);
	if(BS > MAXCHUNKSIZE) {
		snprintf(errmsg, sizeof(errmsg), "CHUNK REQ TOO LARGE: %ld", BS);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	buf = malloc(BS);
	left = (xp->size - xp->offset);
	if(left < BS) {
		bytes = gcfile_read(&xp->gcf, buf, left);
	} else {
		bytes = gcfile_read(&xp->gcf, buf, BS);
	}
	xp->offset += bytes;

	n = snprintf(len, sizeof(len), "%ld", bytes);
	(void) as_zmq_reply_send(r, TSTAT_OK,	strlen(TSTAT_OK)+1,	1);
	(void) as_zmq_reply_send(r, buf,		bytes,				1);
	(void) as_zmq_reply_send(r, len,		n+1,				0);
	free(buf);
}

/*
	snprintf(blocksize, sizeof(blocksize), "%d", BS);
	snprintf(offset, sizeof(offset), "%ld", bytes);
	z = zmq_send(s, TCMD_XFR,	strlen(TCMD_XFR)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, g_uuid,		sizeof(g_uuid),		ZMQ_SNDMORE);
	z = zmq_send(s, offset,		sizeof(offset),		ZMQ_SNDMORE);
	z = zmq_send(s, blocksize,	sizeof(blocksize),	0);
*/
void tpad_xfr(zmq_reply_t *r, zmq_mf_t *msg2, zmq_mf_t *msg3, zmq_mf_t *msg4)
{
	char *uuid, *offset, *bshash;

	uuid   = (char *)msg2->buf;
	offset = (char *)msg3->buf;
	bshash = (char *)msg4->buf;
	if(strlen(uuid) > 0) {
		get_xfer_block(r, uuid, atol(offset), bshash);
		return;
	}
}