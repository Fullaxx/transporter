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

#ifndef __TPAD_ERROR_H__
#define __TPAD_ERROR_H__

#include <stdio.h>
#include "async_zmq_reply.h"

static void tpad_error(zmq_reply_t *r, const char *func, char *msg)
{
	fprintf(stderr, "%s(): %s\n", func, msg);
	(void) as_zmq_reply_send(r, TCMD_ERR, strlen(TCMD_ERR)+1, 1);
	(void) as_zmq_reply_send(r, msg, strlen(msg)+1, 0);
}

#endif
