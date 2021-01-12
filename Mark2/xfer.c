/*
	xfer is an easy to use interface to libgcrypt with FILE operations
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
#include <string.h>

#include "xfer.h"
#include "rnum.h"

xfer_t g_xlist[MAXACTIVE];

void xfer_init(void)
{
	memset(&g_xlist[0], 0, sizeof(g_xlist));
}

xfer_t* xfer_new(char *path, char *mode)
{
	int i, z, openslot;
	xfer_t *xp;

	openslot = 0;
	for(i=0; i<MAXACTIVE; i++) {
		xp = &g_xlist[i];
		if(!GCFILE_ISOPEN(&xp->gcf)) {
			openslot = 1;
			break;
		}
	}

	if(openslot == 0) { return NULL; }

	GCFILE_INIT(&xp->gcf);
	z = gcfile_open(&xp->gcf, path, mode);
	if(z != 0) { return NULL; }

	snprintf(xp->uuid, sizeof(xp->uuid), "%lu", randomul());
	return xp;
}

xfer_t* xfer_find_uuid(char *uuid)
{
	int i;
	xfer_t *xp;

	for(i=0; i<MAXACTIVE; i++) {
		xp = &g_xlist[i];
		if(GCFILE_ISOPEN(&xp->gcf)) {
			if(strncmp(uuid, xp->uuid, 24) == 0) {
				return &g_xlist[i];
			}
		}
	}

	return NULL;
}

void xfer_complete(xfer_t *xp, int del)
{
	char *path;

	gcfile_close(&xp->gcf);
	path = GCFILE_GETPATH(&xp->gcf);
	if(del) { remove(path); }
	GCFILE_INIT(&xp->gcf);
	memset(xp, 0, sizeof(xfer_t));
}
