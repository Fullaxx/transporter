/*
	gcryptfile is an easy to use interface to libgcrypt with FILE operations
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

#ifndef __GCRYPTFILE_H__
#define __GCRYPTFILE_H__

#include <stdio.h>
#include <string.h>
#include <gcrypt.h>
#include <time.h>

typedef struct {
	FILE *f;
	char path[1024+1];
	int is_open;
	gcry_md_hd_t h;
	char errmsg[1280];

	struct timeval timestart;
	unsigned long bytecount;
} gcfile_t;

#define GCFILE_INIT(s) { memset((s), 0, sizeof(gcfile_t)); }
#define GCFILE_ISOPEN(s) ((s)->is_open)
#define GCFILE_GETPATH(s) (&(s)->path[0])
#define GCFILE_GETERRMSG(s) (&(s)->errmsg[0])

int gcfile_open(gcfile_t *, const char *, const char *);
int gcfile_enable(gcfile_t *, int);
size_t gcfile_read(gcfile_t *, void *, size_t);
size_t gcfile_write(gcfile_t *, const void *, size_t);
char* gcfile_get_hash(gcfile_t *, int);
unsigned char* gcfile_get_digest(gcfile_t *, int);
void gcfile_close(gcfile_t *);

unsigned long gcfile_get_bytecount(gcfile_t *gcf);
unsigned long gcfile_get_duration(gcfile_t *gcf);

#endif
