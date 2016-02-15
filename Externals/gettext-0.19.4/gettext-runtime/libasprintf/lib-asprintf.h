/* Library functions for class autosprintf.
   Copyright (C) 2002-2003 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2002.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_VASPRINTF && HAVE_POSIX_PRINTF

/* Get asprintf(), vasprintf() declarations.  */
#include <stdio.h>

#else

/* Get asprintf(), vasprintf() declarations.  */
#include "vasprintf.h"

#endif
