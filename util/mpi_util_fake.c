/**********************************************************************
  mpi_util_fake.c
 **********************************************************************

  mpi_util_fake - mpi_util compilation kludge.
  Copyright ©2002-2003, Stewart Adcock <stewart@linux-domain.com>
  All rights reserved.

  The latest version of this program should be available at:
  http://gaul.sourceforge.net/

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.  Alternatively, if your project
  is incompatible with the GPL, I will probably agree to requests
  for permission to use the terms of any other license.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY WHATSOEVER.

  A full copy of the GNU General Public License should be in the file
  "COPYING" provided with this distribution; if not, see:
  http://www.gnu.org/

 **********************************************************************

  Synopsis:     This is a nasty kludge required for 'neat' compilation
		of mixed MPI and non-MPI code via automake and
		friends.

  Updated:      03 Oct 2002 SAA	Removed unneeded #include <stdlib.h>
		15 Mar 2002 SAA	Nasty hack implemented.

 **********************************************************************/

#define NO_PARALLEL

#include "gaul_util.h"

/*
 * Forced over-ride of the PARALLEL constant.
 */
#undef PARALLEL
#define PARALLEL	0


#include "mpi_util.c"

