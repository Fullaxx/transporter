/*
	xfer is an easy to use interface to libgcrypt with FILE operations
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

#ifndef __TPAD_XFER_H__
#define __TPAD_XFER_H__

#include "gcryptfile.h"

// MAXACTIVE was set to 256, but gcrypt was giving us errors after about 70 or so
// maybe we were running out of secure memory?
#define MAXACTIVE (64)
#define UUIDSIZE (64)

// If we wanted exclusive access to the files being transferred,
// should we reaplce the uuid with filename?

typedef struct {
	gcfile_t gcf;
	char uuid[UUIDSIZE];
	long size;
	long offset;
	//time_t last;
} xfer_t;

xfer_t* xfer_new(char *path, char *mode);
xfer_t* xfer_find_uuid(char *uuid);
void xfer_complete(xfer_t *xp, int del);

#endif
