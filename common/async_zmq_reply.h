/*
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

#ifndef __ASYNC_ZMQ_REPLY_H__
#define __ASYNC_ZMQ_REPLY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "async_zmq_mpm.h"

#define AS_ZMQ_REPLY_THREADNAME ("zmq_reply_thread")

typedef struct {
	void *zContext;
	void *zSocket;
	int connected;
	int do_close;
	int closed;
} zmq_reply_t;

#define ZR_READ_CALLBACK(CB) void (CB)(zmq_reply_t *, zmq_mf_t **, int, void *);

typedef struct {
	zmq_reply_t *q;
	ZR_READ_CALLBACK(*cb);
	void *user_data;
} ZRParam_t;

int as_zmq_reply_send(zmq_reply_t *reply, void *buf, int len, int more);
zmq_reply_t* as_zmq_reply_create(char *zSockAddr, void *func, int recv_hwm, int send_hwm, int do_connect, void *user);
void as_zmq_reply_destroy(zmq_reply_t *reply);

#ifdef __cplusplus
}
#endif

#endif
