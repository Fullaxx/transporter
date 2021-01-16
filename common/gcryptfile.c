/*
	gcryptfile is an easy to use interface to libgcrypt with FILE operations
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

// Ubuntu Dependencies: sudo apt-get install libgcrypt-dev

// Compile with:
// gcc -Wall myprog.c gcryptfile.c -o myprog -lgcrypt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gcrypt.h>

#include "gcryptfile.h"

// return 0 on success
// return non-zero on error
int gcfile_open(gcfile_t *gcf, const char *path, const char *mode)
{
	gcry_error_t gcerr;

	if(gcf->is_open) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "gcfile_open() failed: file is open");
		return -1;
	}

	gcf->f = fopen(path, mode);
	if(!gcf->f) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "fopen(%s, %s) failed: %s", path, mode, strerror(errno));
		return -2;
	}
	snprintf(gcf->path, sizeof(gcf->path), "%s", path);
	gcf->is_open = 1;

	// We will init the context with GCRY_MD_NONE
	// The user can select which algorithms they want to enable with gcfile_enable()
	gcerr = gcry_md_open(&gcf->h, GCRY_MD_NONE, GCRY_MD_FLAG_SECURE);
	if(gcerr) {
		gcfile_close(gcf);
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "gcry_md_open() failed: %s", gcry_strerror(gcerr));
		return -3;
	}

	gettimeofday(&gcf->timestart, NULL);
	return 0;
}

// return 0 on success
// return non-zero on error
int gcfile_enable(gcfile_t *gcf, int alg)
{
	gcry_error_t gcerr;

	if(!gcf->is_open) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "gcfile_enable(%d) failed: file is not open", alg);
		return -1;
	}

	if(!gcf->h) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "gcfile_enable(%d) failed: gcrypt handle is not active", alg);
		return -2;
	}

	gcerr = gcry_md_enable(gcf->h, alg);
	if(gcerr) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "gcry_md_enable(%d) failed: %s", alg, gcry_strerror(gcerr));
		return -2;
	}
	return 0;
}

size_t gcfile_read(gcfile_t *gcf, void *ptr, size_t nmemb)
{
	size_t bytes;

	if(!gcf->is_open) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "gcfile_read() failed: file is not open");
		return 0;
	}

	if(feof(gcf->f)) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "gcfile_read() failed: EOF");
		return 0;
	}

	bytes = fread(ptr, 1, nmemb, gcf->f);
	if(bytes == 0) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "fread(ptr, 1, %lu, %s) failed: %s",
													nmemb, gcf->path, strerror(errno));
	} else {
		gcry_md_write(gcf->h, ptr, bytes);
		gcf->bytecount += bytes;
	}
	return bytes;
}

size_t gcfile_write(gcfile_t *gcf, const void *ptr, size_t nmemb)
{
	size_t bytes;

	if(!gcf->is_open) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "gcfile_write() failed: file is not open");
		return 0;
	}

	bytes = fwrite(ptr, 1, nmemb, gcf->f);
	if(bytes != nmemb) {
		snprintf(gcf->errmsg, sizeof(gcf->errmsg), "fwrite(ptr, 1, %lu, %s) failed: %s",
													nmemb, gcf->path, strerror(errno));
	} else {
		gcry_md_write(gcf->h, ptr, bytes);
		gcf->bytecount += bytes;
	}
	return bytes;
}

// This must be free()'d
char* gcfile_get_hash(gcfile_t *gcf, int alg)
{
	unsigned int dsize;
	unsigned char *digest;
	int i, hash_len;
	char *hash = NULL;

	dsize = gcry_md_get_algo_dlen(alg);
	digest = gcfile_get_digest(gcf, alg);
	if(digest) {
		hash_len = (dsize*2)+1;
		hash = malloc(hash_len);
		memset(hash, 0, hash_len);
		for(i=0; i<dsize; i++) { sprintf(&hash[i*2], "%02x", digest[i]); }
	}
	return hash;
}

unsigned char* gcfile_get_digest(gcfile_t *gcf, int alg)
{
	return gcry_md_read(gcf->h, alg);
}

void gcfile_close(gcfile_t *gcf)
{
/** h should not be used after a call to this function.
	A NULL passed as h is ignored.
	The function also zeroises all sensitive information associated with this handle. */
	gcry_md_close(gcf->h);
	gcf->h = NULL;

	if(gcf->f) { fclose(gcf->f); }
	gcf->is_open = 0;
	gcf->f = NULL;

	// Dont wipe the errmsg, we might need it in gcfile_open()
}

unsigned long gcfile_get_bytecount(gcfile_t *gcf)
{
	return gcf->bytecount;
}

unsigned long gcfile_get_duration(gcfile_t *gcf)
{
	int z;
	struct timeval timestop;
	unsigned long usec_duration;

	z = gettimeofday(&timestop, NULL);
	if(z != 0) { return 0; }
	usec_duration = (timestop.tv_sec - gcf->timestart.tv_sec)*1e6;
	usec_duration += (long)timestop.tv_usec - (long)gcf->timestart.tv_usec;

	return usec_duration;
}
