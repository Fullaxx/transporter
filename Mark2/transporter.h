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

#ifndef __TRANSPORTER_H__
#define __TRANSPORTER_H__

#define MAXCHUNKSIZE (524288)

#define TSTAT_ERR "ERR"
#define TSTAT_OK  "OK"

//#define TCMD_ABS "ABS"
#define TCMD_CMD "CMD"
#define TCMD_GET "GET"
#define TCMD_PUT "PUT"
#define TCMD_DEL "DEL"
#define TCMD_XFR "XFR"

#define COUNTCMD "::filecount()::"
#define RANDOMCMD "::randomfile()::"
#define OLDESTCMD "::oldestfile()::"
#define NEWESTCMD "::newestfile()::"
#define LARGESTCMD "::largestfile()::"
#define SMALLESTCMD "::smallestfile()::"

#define RNDMETHOD (0)
#define OLDMETHOD (1)
#define NEWMETHOD (2)
#define LRGMETHOD (3)
#define SMLMETHOD (4)

#endif
