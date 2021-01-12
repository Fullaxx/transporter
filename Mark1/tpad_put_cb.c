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
#include "gcryptfile.h"

static void put_file(zmq_reply_t *r, char *filename, void *buf, unsigned long size)
{
	char errmsg[2048];
	gcfile_t gcf;
	int z;
	size_t bytes;
	char *hash;

	GCFILE_INIT(&gcf);
	z = gcfile_open(&gcf, filename, "w");
	if(z != 0) {
		snprintf(errmsg, sizeof(errmsg), "%s\n", gcfile_geterrmsg(&gcf));
		tpad_error(r, __func__, errmsg);
		return;
	}

	z = gcfile_enable(&gcf, TPAD_HASH_ALG);
	if(z != 0) {
		gcfile_close(&gcf);
		snprintf(errmsg, sizeof(errmsg), "%s\n", gcfile_geterrmsg(&gcf));
		tpad_error(r, __func__, errmsg);
		return;
	}

	bytes = gcfile_write(&gcf, buf, size);
	if(bytes != size) {
		gcfile_close(&gcf);
		snprintf(errmsg, sizeof(errmsg), "%s\n", gcfile_geterrmsg(&gcf));
		tpad_error(r, __func__, errmsg);
		return;
	}

	hash = gcfile_get_hash(&gcf, TPAD_HASH_ALG);
	(void) as_zmq_reply_send(r, TCMD_OK, strlen(TCMD_OK)+1, 1);
	(void) as_zmq_reply_send(r, hash, strlen(hash)+1, 0);
	free(hash);
	gcfile_close(&gcf);
	GCFILE_INIT(&gcf)
}

/* beam.c
zmq_send(g_zReq, TCMD_PUT,	strlen(TCMD_PUT)+1,	ZMQ_SNDMORE);
zmq_send(g_zReq, filename,	strlen(filename)+1,	ZMQ_SNDMORE);
zmq_send(g_zReq, buf,		filesize,			0);
*/
void tpad_put_cb(zmq_reply_t *r, zmq_mf_t **mpa, int msgcnt, void *user_data)
{
	zmq_mf_t *command_msg;	// Command (String)
	zmq_mf_t *filename_msg;	// Filename (String)
	zmq_mf_t *contents_msg;	// File Contents (data)
	char errmsg[1400];
	char *cmd;
	char *filename;

	if(!mpa) { return; }
	if(msgcnt != 3) { return; }

	command_msg = mpa[0];	if(!command_msg) return;
	filename_msg = mpa[1];	if(!filename_msg) return;
	contents_msg = mpa[2];	if(!contents_msg) return;

	cmd = (char *)command_msg->buf;
	if(strcmp(cmd, TCMD_PUT) != 0) {
		snprintf(errmsg, sizeof(errmsg), "INVALID COMMAND %s", cmd);
		tpad_error(r, __func__, errmsg);
		return;
	}

	filename = (char *)filename_msg->buf;

#ifdef DEBUG
	printf("%s(): %s (%lu) %s\n", __func__, cmd, contents_msg->size, filename);
#endif

	if(file_security_check(filename)) {
		snprintf(errmsg, sizeof(errmsg), "INVALID FILENAME %s", filename);
		tpad_error(r, __func__, errmsg);
		return;
	}

	put_file(r, (char *)filename_msg->buf, contents_msg->buf, contents_msg->size);
}
