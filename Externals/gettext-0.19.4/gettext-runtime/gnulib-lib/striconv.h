/* Charset conversion.
   Copyright (C) 2001-2004, 2006-2007, 2009-2014 Free Software Foundation, Inc.
   Written by Bruno Haible and Simon Josefsson.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

#ifndef _STRICONV_H
#define _STRICONV_H

#include <stddef.h>
#if HAVE_ICONV
#include <iconv.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


#if HAVE_ICONV

/* Convert an entire string from one encoding to another, using iconv.
   The original string is at [SRC,...,SRC+SRCLEN-1].
   The conversion descriptor is passed as CD.
   *RESULTP and *LENGTH should initially be a scratch buffer and its size,
   or *RESULTP can initially be NULL.
   May erase the contents of the memory at *RESULTP.
   Return value: 0 if successful, otherwise -1 and errno set.
   If successful: The resulting string is stored in *RESULTP and its length
   in *LENGTHP.  *RESULTP is set to a freshly allocated memory block, or is
   unchanged if no dynamic memory allocation was necessary.  */
extern int mem_cd_iconv (const char *src, size_t srclen, iconv_t cd,
                         char **resultp, size_t *lengthp);

/* Convert an entire string from one encoding to another, using iconv.
   The original string is the NUL-terminated string starting at SRC.
   The conversion descriptor is passed as CD.  Both the "from" and the "to"
   encoding must use a single NUL byte at the end of the string (i.e. not
   UCS-2, UCS-4, UTF-16, UTF-32).
   Allocate a malloced memory block for the result.
   Return value: the freshly allocated resulting NUL-terminated string if
   successful, otherwise NULL and errno set.  */
extern char * str_cd_iconv (const char *src, iconv_t cd);

#endif

/* Convert an entire string from one encoding to another, using iconv.
   The original string is the NUL-terminated string starting at SRC.
   Both the "from" and the "to" encoding must use a single NUL byte at the
   end of the string (i.e. not UCS-2, UCS-4, UTF-16, UTF-32).
   Allocate a malloced memory block for the result.
   Return value: the freshly allocated resulting NUL-terminated string if
   successful, otherwise NULL and errno set.  */
extern char * str_iconv (const char *src,
                         const char *from_codeset, const char *to_codeset);


#ifdef __cplusplus
}
#endif


#endif /* _STRICONV_H */
