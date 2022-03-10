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

#include <stdio.h>
#include <stdlib.h>

unsigned long randomul(void)
{
	FILE *f;
	unsigned long rnum = 0;

	f = fopen("/dev/urandom", "r");
	if(f) {
		(void) fread(&rnum, sizeof(unsigned long), 1, f);
		fclose(f);
	} else {
		rnum = ((long)rand() << 32) | rand();
	}

	return rnum;
}

int rpick(int bound)
{
	return (int)(randomul() % bound);
}
