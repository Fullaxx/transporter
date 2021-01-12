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

#include <gcrypt.h>
char* calc_file_hash(char *filename, size_t filesize)
{
	FILE *f;
	char *errfmt;
	char *buf;
	int i, n, alg;
	int dig_len, hash_len;
	unsigned char *digest;
	char *hash;

	if(sizeof(size_t) == 4) errfmt = "malloc(%u) failed!\n";
	else					errfmt = "malloc(%lu) failed!\n";

	f = fopen(filename, "r");
	if(!f) { fprintf(stderr, "fopen(%s, r) failed: %s\n", filename, strerror(errno)); return NULL; }

	buf = malloc(filesize);
	if(!buf) { fclose(f); fprintf(stderr, errfmt, filesize); return NULL; }

	n = fread(buf, filesize, 1, f);
	if(n != 1) { fclose(f); free(buf); fprintf(stderr, errfmt, filesize); return NULL; }

	alg = GCRY_MD_WHIRLPOOL;
	dig_len = gcry_md_get_algo_dlen(alg);
	digest = alloca(dig_len);
	gcry_md_hash_buffer(alg, digest, buf, filesize);

	hash_len = (dig_len*2)+1;
	hash = calloc(1, hash_len);
	for(i=0; i<dig_len; i++) { sprintf(&hash[i*2], "%02x", digest[i]); }

#ifdef DEBUG
	printf("%s(): %s %s (%s)\n", __func__, "HASH", filename, hash);
#endif

	free(buf);
	fclose(f);
	return hash;	// This must be free()'d
}

/* absorb.c
zmq_send(g_zDELReq, TCMD_DEL,	strlen(TCMD_DEL)+1,	ZMQ_SNDMORE);
zmq_send(g_zDELReq, filename,	strlen(filename)+1,	ZMQ_SNDMORE);
zmq_send(g_zDELReq, filesize,	strlen(filesize)+1,	ZMQ_SNDMORE);
zmq_send(g_zDELReq, filehash,	strlen(filehash)+1,	0);
*/
void tpad_del_cb(zmq_reply_t *r, zmq_mf_t **mpa, int msgcnt, void *user_data)
{
	int z;
	char *cmd;
	char *filename;
	char errmsg[1400];
	long local_filesize;
	long remote_filesize;
	char *local_hash;
	char *remote_hash;
	zmq_mf_t *command_msg;	// Command (String)
	zmq_mf_t *filename_msg;	// Filename (String)
	zmq_mf_t *filesize_msg;	// Filesize (String)
	zmq_mf_t *filehash_msg;	// Filehash (String)

	if(!mpa) { return; }
	if(msgcnt != 4) { return; }

	command_msg = mpa[0];	if(!command_msg) return;
	filename_msg = mpa[1];	if(!filename_msg) return;
	filesize_msg = mpa[2];	if(!filesize_msg) return;
	filehash_msg = mpa[3];	if(!filehash_msg) return;

	cmd = (char *)command_msg->buf;
	if(strcmp(cmd, TCMD_DEL) != 0) {
		snprintf(errmsg, sizeof(errmsg), "INVALID COMMAND: %s", cmd);
		tpad_error(r, __func__, errmsg);
		return;
	}

	filename = (char *)filename_msg->buf;
	remote_filesize = atol(filesize_msg->buf);
	remote_hash = (char *)filehash_msg->buf;

#ifdef DEBUG
	printf("%s(): %s %s (%lu)\n", __func__, cmd, filename, remote_filesize);
#endif

	// Get the size of the local file
	local_filesize = file_size(filename, 1);
	if(local_filesize < 0) {
		snprintf(errmsg, sizeof(errmsg), "%s: filesize(%ld) < 0", filename, local_filesize);
		tpad_error(r, __func__, errmsg);
		return;
	}

	// If the remote file size != local file size, error
	if(local_filesize != remote_filesize) {
		snprintf(errmsg, sizeof(errmsg), "%s: local_filesize(%ld) != remote_filesize(%ld)", filename, local_filesize, remote_filesize);
		tpad_error(r, __func__, errmsg);
		return;
	}

	// If the hashes don't match, error
	local_hash = calc_file_hash(filename, local_filesize);
	z = strcmp(local_hash, remote_hash);
	free(local_hash);
	if(z != 0) {
		snprintf(errmsg, sizeof(errmsg), "%s: local_hash(%s) != remote_hash(%s)", filename, local_hash, remote_hash);
		tpad_error(r, __func__, errmsg);
		return;
	}

	// File sizes match, process the DEL request
	remove(filename);
	(void) as_zmq_reply_send(r, TCMD_OK, strlen(TCMD_OK)+1, 0);
}
