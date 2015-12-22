/* Formatted output to strings.
   Copyright (C) 1999, 2002, 2006-2007 Free Software Foundation, Inc.

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

/* Specification.  */
#ifdef IN_LIBASPRINTF
# include "vasprintf.h"
#else
# include <stdio.h>
#endif

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "vasnprintf.h"

/* Some systems, like OSF/1 4.0 and Woe32, don't have EOVERFLOW.  */
#ifndef EOVERFLOW
# define EOVERFLOW E2BIG
#endif

int
vasprintf (char **resultp, const char *format, va_list args)
{
  size_t length;
  char *result = vasnprintf (NULL, &length, format, args);
  if (result == NULL)
    return -1;

  if (length > INT_MAX)
    {
      free (result);
      errno = EOVERFLOW;
      return -1;
    }

  *resultp = result;
  /* Return the number of resulting bytes, excluding the trailing NUL.  */
  return length;
}
