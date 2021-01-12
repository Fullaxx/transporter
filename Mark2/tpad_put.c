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
#include <errno.h>

#include "async_zmq_reply.h"
#include "transporter.h"
#include "futils.h"
#include "tpad_error.h"
#include "gchelper.h"
#include "xfer.h"

extern int g_noclobber;

/*	beam.c
	z = zmq_send(s, TCMD_PUT,	strlen(TCMD_PUT)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, uuid,		strlen(uuid)+1,		ZMQ_SNDMORE);
	z = zmq_send(s, filename,	strlen(filename)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, filesize,	strlen(filesize)+1,	0);
*/
void tpad_put_new(zmq_reply_t *r, char *filename, char *filesize)
{
	int z, file_exists;
	long size;
	xfer_t *xp;
	char offset[24];
	char errmsg[1536];

#ifdef DEBUG
	printf("%s(): %s %s(%s)\n", __func__, "PUT", filename, filesize);
#endif

	if(file_security_check(filename)) {
		snprintf(errmsg, sizeof(errmsg), "INVALID FILENAME %s", filename);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	size = atol(filesize);
	if(size <= 0) {
		snprintf(errmsg, sizeof(errmsg), "BAD FILESIZE: %s", filesize);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	// If server is set to no_clobber, error if file exists
	if(g_noclobber) {
		file_exists = is_regfile(filename, 0);
		if(file_exists != -1) {
			snprintf(errmsg, sizeof(errmsg), "Server will not clobber %s", filename);
			tpad_error(r, __func__, errmsg, NULL);
			return;
		}
	}

	// Find an open transfer slot, then
	// Make a new entry with filename, filesize, uuid, current offset
	xp = xfer_new(filename, "w");
	if(!xp) {
		snprintf(errmsg, sizeof(errmsg), "Could not get open xfer slot for %s", filename);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

#ifdef DEBUG
	printf("%s(): %s %s(%s)\n", __func__, "XFER", filename, xp->uuid);
#endif

/*
	GCFILE_INIT(&newfile);
	z = gcfile_open(&newfile, );
	if(z != 0) {
		snprintf(errmsg, sizeof(errmsg), "%s", gcfile_geterrmsg(&newfile));
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}
*/

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

	snprintf(offset, sizeof(offset), "%ld", xp->offset);
	(void) as_zmq_reply_send(r, TSTAT_OK,	strlen(TSTAT_OK)+1, 1);
	(void) as_zmq_reply_send(r, xp->uuid,	strlen(xp->uuid)+1, 1);
	(void) as_zmq_reply_send(r, offset,		strlen(offset)+1, 0);
}

/*	beam.c
	z = zmq_send(s, TCMD_PUT,	strlen(TCMD_PUT)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, g_uuid,		strlen(g_uuid)+1,	ZMQ_SNDMORE);
	z = zmq_send(s, buf,		bytes,				ZMQ_SNDMORE);
	z = zmq_send(s, empty,		1,	0);
*/
void tpad_put_chunk(zmq_reply_t *r, char *uuid, zmq_mf_t *msg3)
{
	xfer_t *xp;
	long written;
	char offset[24];
	char errmsg[64];
	char hash[TPAD_HASH_SIZE+1];
	char *hashptr;

	// FIND UUID
	xp = xfer_find_uuid(uuid);
	if(!xp) {
		snprintf(errmsg, sizeof(errmsg), "UNKNOWN UUID: %s", uuid);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

#ifdef DEBUG
	printf("%s(): %s %s(%lu)\n", __func__, "PUT", GCFILE_GETPATH(&xp->gcf), msg3->size);
#endif

	written = gcfile_write(&xp->gcf, msg3->buf, msg3->size);
	if(written == msg3->size) {
		xp->offset += written;
		snprintf(offset, sizeof(offset), "%ld", xp->offset);
	} else {
		snprintf(errmsg, sizeof(errmsg), "written(%ld) != bytes(%ld)", written, msg3->size);
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}

	// Check for completion
	memset(hash, 0, sizeof(hash));
	if(xp->offset == xp->size) {
		hashptr = gcfile_get_hash(&xp->gcf, TPAD_HASH_ALG);
		snprintf(hash, sizeof(hash), "%s", hashptr);
		free(hashptr);
		xfer_complete(xp, 0);
	}

	(void) as_zmq_reply_send(r, TSTAT_OK,	strlen(TSTAT_OK)+1, 1);
	(void) as_zmq_reply_send(r, offset,		strlen(offset)+1, 1);
	(void) as_zmq_reply_send(r, hash,		strlen(hash)+1, 0);
}

void tpad_put(zmq_reply_t *r, zmq_mf_t *msg2, zmq_mf_t *msg3, zmq_mf_t *msg4)
{
	char *uuid, *filename, *filesize;

	uuid = (char *)msg2->buf;

/*
#ifdef DEBUG
	printf("%s(): %s %s\n", __func__, "PUT", uuid);
#endif
*/

	if(strlen(uuid) == 0) {
		filename = (char *)msg3->buf;
		filesize = (char *)msg4->buf;
		tpad_put_new(r, filename, filesize);
	} else {
		tpad_put_chunk(r, uuid, msg3);
	}
}
