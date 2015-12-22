/* Find the length of STRING + 1, but scan at most MAXLEN bytes.
   Copyright (C) 2005, 2009-2014 Free Software Foundation, Inc.

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

#ifndef _STRNLEN1_H
#define _STRNLEN1_H

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


/* Find the length of STRING + 1, but scan at most MAXLEN bytes.
   If no '\0' terminator is found in that many characters, return MAXLEN.  */
/* This is the same as strnlen (string, maxlen - 1) + 1.  */
extern size_t strnlen1 (const char *string, size_t maxlen)
  _GL_ATTRIBUTE_PURE;


#ifdef __cplusplus
}
#endif


#endif /* _STRNLEN1_H */
