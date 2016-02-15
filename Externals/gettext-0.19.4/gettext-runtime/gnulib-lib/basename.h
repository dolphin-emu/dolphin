/* Pathname hacking.
   Copyright (C) 2001-2003, 2010 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef _BASENAME_H
#define _BASENAME_H

/* This is where basename() is declared.  */
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif


#if !(__GLIBC__ >= 2 || defined __UCLIBC__)
/* When not using the GNU libc we use the basename implementation we
   provide here.  */
extern char *gnu_basename (const char *);
#define basename(Arg) gnu_basename (Arg)
#endif


#ifdef __cplusplus
}
#endif


#endif /* _BASENAME_H */
