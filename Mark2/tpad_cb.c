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

#include "async_zmq_reply.h"
#include "transporter.h"
#include "tpad_error.h"

void tpad_cmd(zmq_reply_t *r, zmq_mf_t *msg2, zmq_mf_t *msg3, zmq_mf_t *msg4);
void tpad_put(zmq_reply_t *r, zmq_mf_t *msg2, zmq_mf_t *msg3, zmq_mf_t *msg4);
void tpad_get(zmq_reply_t *r, zmq_mf_t *msg2, zmq_mf_t *msg3, zmq_mf_t *msg4);
void tpad_xfr(zmq_reply_t *r, zmq_mf_t *msg2, zmq_mf_t *msg3, zmq_mf_t *msg4);

void tpad_cb(zmq_reply_t *r, zmq_mf_t **mpa, int msgcnt, void *user_data)
{
	char *cmd;
	char errmsg[1400];
	zmq_mf_t *msg1;
	zmq_mf_t *msg2;
	zmq_mf_t *msg3;
	zmq_mf_t *msg4;

	if(!mpa) { return; }
	if(msgcnt != 4) { return; }

	msg1 = mpa[0];	if(!msg1) { return; }
	msg2 = mpa[1];	if(!msg2) { return; }
	msg3 = mpa[2];	if(!msg3) { return; }
	msg4 = mpa[3];	if(!msg4) { return; }

	cmd = (char *)msg1->buf;
	if(msg1->size != 4) {
		snprintf(errmsg, sizeof(errmsg), "INVALID COMMAND");
		tpad_error(r, __func__, errmsg, NULL);
		return;
	}
	cmd[3] = 0;

/*
#ifdef DEBUG
	printf("%s(): %s\n", __func__, cmd);
#endif
*/

	if(strcmp(cmd, TCMD_CMD) == 0) {
		tpad_cmd(r, msg2, msg3, msg4);
	} else if(strcmp(cmd, TCMD_PUT) == 0) {
		tpad_put(r, msg2, msg3, msg4);
	} else if(strcmp(cmd, TCMD_GET) == 0) {
		tpad_get(r, msg2, msg3, msg4);
	} else if(strcmp(cmd, TCMD_XFR) == 0) {
		tpad_xfr(r, msg2, msg3, msg4);
	} else {
		snprintf(errmsg, sizeof(errmsg), "INVALID COMMAND: %s", cmd);
		tpad_error(r, __func__, errmsg, NULL);
	}
}
