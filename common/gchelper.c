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
//#include <stdlib.h>
//#include <unistd.h>
#include <gcrypt.h>

#include "gchelper.h"

int tpad_gcinit(char *vers)
{

/*  If the application requests FIPS mode using the control command GCRYCTL_FORCE_FIPS_MODE.
 *  This must be done prior to any initialization (i.e. before gcry_check_version). */
#ifdef TPAD_USING_FIPS
	gcry_control (GCRYCTL_FORCE_FIPS_MODE);
#endif

/*  Version check should be the very first call because it
 *  makes sure that important subsystems are initialized.
 *  #define NEED_LIBGCRYPT_VERSION to the minimum required version. */
	if (!gcry_check_version (vers)) {
		fprintf (stderr, "libgcrypt is too old (need %s, have %s)\n",
		vers, gcry_check_version (NULL));
		return -1;
	}

/*  We don't want to see any warnings, e.g. because we have not yet
 *  parsed program options which might be used to suppress such warnings. */
	gcry_control (GCRYCTL_SUSPEND_SECMEM_WARN);

/*  ... If required, other initialization goes here.  Note that the
 *  process might still be running with increased privileges and that
 *  the secure memory has not been initialized.  */

/*  Allocate a pool of 16k secure memory.  This makes the secure memory
	available and also drops privileges where needed.  Note that by
	using functions like gcry_xmalloc_secure and gcry_mpi_snew Libgcrypt
	may expand the secure memory pool with memory which lacks the
	property of not being swapped out to disk.   */
#ifdef TPAD_USING_SECURE_MEMORY
	gcry_control (GCRYCTL_INIT_SECMEM, TPAD_SECMEM_SIZE, 0);
#else
	gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
#endif

/*  It is now okay to let Libgcrypt complain when there was/is
 *  a problem with the secure memory. */
	gcry_control (GCRYCTL_RESUME_SECMEM_WARN);

/*  ... If required, other initialization goes here.  */

/*  Tell Libgcrypt that initialization has completed. */
	gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);

	return 0;
}

/*
void tpad_gcerror(const char *what, gcry_error_t err, int exitcode)
{
	g_shutdown = 1;
	usleep(1000);

	fprintf(stderr, "%s failed: %s\n", what, gcry_strerror(err));
	exit(exitcode);
}
*/
