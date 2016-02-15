/* Return the name-within-directory of a file name.
   Copyright (C) 1996-1999, 2000-2002, 2004, 2006, 2010, 2012 Free Software Foundation, Inc.

   NOTE: The canonical source of this file is maintained with the GNU C Library.
   Bugs can be reported to bug-glibc@gnu.org.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3 of the License, or any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include "basename.h"

#if !(__GLIBC__ >= 2 || defined __UCLIBC__)

#include <stdio.h>
#include <assert.h>

#if defined _WIN32 || defined __WIN32__ || defined __CYGWIN__ || defined __EMX__ || defined __DJGPP__
  /* Win32, Cygwin, OS/2, DOS */
# define HAS_DEVICE(P) \
    ((((P)[0] >= 'A' && (P)[0] <= 'Z') || ((P)[0] >= 'a' && (P)[0] <= 'z')) \
     && (P)[1] == ':')
# define FILE_SYSTEM_PREFIX_LEN(P) (HAS_DEVICE (P) ? 2 : 0)
# define ISSLASH(C) ((C) == '/' || (C) == '\\')
#endif

#ifndef FILE_SYSTEM_PREFIX_LEN
# define FILE_SYSTEM_PREFIX_LEN(Filename) 0
#endif

#ifndef ISSLASH
# define ISSLASH(C) ((C) == '/')
#endif

#ifndef _LIBC
/* We cannot generally use the name 'basename' since XPG defines an unusable
   variant of the function but we cannot use it.  */
# undef basename
# define basename gnu_basename
#endif

/* In general, we can't use the builtin 'basename' function if available,
   since it has different meanings in different environments.
   In some environments the builtin 'basename' modifies its argument.
   If NAME is all slashes, be sure to return '/'.  */

char *
basename (char const *name)
{
  char const *base = name += FILE_SYSTEM_PREFIX_LEN (name);
  int all_slashes = 1;
  char const *p;

  for (p = name; *p; p++)
    {
      if (ISSLASH (*p))
        base = p + 1;
      else
        all_slashes = 0;
    }

  /* If NAME is all slashes, arrange to return '/'.  */
  if (*base == '\0' && ISSLASH (*name) && all_slashes)
    --base;

  /* Make sure the last byte is not a slash.  */
  assert (all_slashes || !ISSLASH (*(p - 1)));

  return (char *) base;
}

#endif
