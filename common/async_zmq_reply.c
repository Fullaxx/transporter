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

#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <zmq.h>
#include "async_zmq_reply.h"

int as_zmq_reply_send(zmq_reply_t *reply, void *buf, int len, int more)
{
	int n, flags=0;

	if(more) flags = ZMQ_SNDMORE;
	n = zmq_send(reply->zSocket, buf, len, flags);
#ifdef DEBUG
	if(n != len) {
		fprintf(stderr, "zmq_send() returned %d!\n", n);
	}
#endif
	return n;
}

static void* zmq_reply_thread(void *param)
{
	int mpi, msgsize, more, i;
	size_t smore=sizeof(more);
	zmq_msg_t zMessage;
	zmq_mf_t **mpa;
	zmq_mf_t *thispart;
	ZRParam_t *p = (ZRParam_t *)param;
	zmq_reply_t *reply = p->q;

	prctl(PR_SET_NAME, AS_ZMQ_REPLY_THREADNAME, 0, 0, 0);

	// we repeat until the connection has been closed
	zmq_msg_init(&zMessage);
	reply->connected = 1;
	while(!reply->do_close) {
		msgsize = zmq_msg_recv(&zMessage, reply->zSocket, ZMQ_DONTWAIT);
		if(msgsize == -1) {
			if(errno == EAGAIN) { usleep(100); continue; }
			else break;
		}

		mpa = calloc(AS_ZMQ_MAX_PARTS, sizeof(zmq_mf_t *));
		mpi = 0;
		do {
			if(mpi > 0) msgsize = zmq_msg_recv(&zMessage, reply->zSocket, 0);
			thispart = mpa[mpi++] = calloc(1, sizeof(zmq_mf_t));
			thispart->size = msgsize;
			thispart->buf = calloc(1, msgsize);
			memcpy(thispart->buf, zmq_msg_data(&zMessage), msgsize);
			zmq_msg_close(&zMessage); zmq_msg_init(&zMessage);
			zmq_getsockopt(reply->zSocket, ZMQ_RCVMORE, &more, &smore);
		} while(more && (mpi<AS_ZMQ_MAX_PARTS));

		p->cb(reply, mpa, mpi, p->user_data);

		for(i=0; i<AS_ZMQ_MAX_PARTS; i++) {
			if(mpa[i]) { free(mpa[i]->buf); free(mpa[i]); }
		}
		free(mpa);
	}

	zmq_msg_close(&zMessage);
	p->cb(reply, NULL, 0, p->user_data);
	reply->closed = 1;
	free(p);
#ifdef DEBUG
	printf("%s() EXIT\n", __func__);
#endif
	return (NULL);
}

static int as_zmq_reply_attach(zmq_reply_t *reply, void *func, void *user)
{
	pthread_t thr_id;
	ZRParam_t *p = (ZRParam_t *)calloc(1, sizeof(ZRParam_t));

	p->q = reply;
	p->cb = func;
	p->user_data = user;

	if( pthread_create(&thr_id, NULL, &zmq_reply_thread, p) ) goto bail;
	if( pthread_detach(thr_id) ) goto bail;

	return 0;

bail:
	free(p);
	return -1;
}

zmq_reply_t* as_zmq_reply_create(char *zSockAddr, void *func, int recv_hwm, int send_hwm, int do_connect, void *user)
{
	int r;
	zmq_reply_t *reply = calloc(1, sizeof(zmq_reply_t));
	if(!reply) return NULL;

	reply->zContext = zmq_ctx_new();
	reply->zSocket = zmq_socket(reply->zContext, ZMQ_REP);

	r = zmq_setsockopt(reply->zSocket, ZMQ_RCVHWM, &recv_hwm, sizeof(recv_hwm));
	if(r != 0) {
		goto bail;
	}

	r = zmq_setsockopt(reply->zSocket, ZMQ_SNDHWM, &send_hwm, sizeof(send_hwm));
	if(r != 0) {
		goto bail;
	}

	if(do_connect) r = zmq_connect(reply->zSocket, zSockAddr);
	else		r = zmq_bind(reply->zSocket, zSockAddr);
	if(r != 0) {
		goto bail;
	}

	r = as_zmq_reply_attach(reply, func, user);
	if(r != 0) {
		goto bail;
	}

	return reply;

bail:
	zmq_close(reply->zSocket);
	zmq_ctx_term(reply->zContext);
	free(reply);
	return NULL;
}

void as_zmq_reply_destroy(zmq_reply_t *reply)
{
	if(!reply) return;

	if(reply->connected) {
		reply->do_close = 1;
		while(!reply->closed) usleep(1000);
		zmq_close(reply->zSocket);
		zmq_ctx_term(reply->zContext);
	}
	
	free(reply);
}
