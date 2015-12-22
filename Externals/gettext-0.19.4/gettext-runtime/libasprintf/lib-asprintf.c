/* Library functions for class autosprintf.
   Copyright (C) 2002-2003, 2006 Free Software Foundation, Inc.
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

#include <config.h>

#if !(HAVE_VASPRINTF && HAVE_POSIX_PRINTF)

#define STATIC static

/* Define auxiliary functions declared in "printf-args.h".  */
#include "printf-args.c"

/* Define auxiliary functions declared in "printf-parse.h".  */
#include "printf-parse.c"

/* Define functions declared in "vasnprintf.h".  */
#include "vasnprintf.c"
#include "asnprintf.c"

/* Define functions declared in "vasprintf.h".  */
#include "vasprintf.c"
#include "asprintf.c"

#endif
