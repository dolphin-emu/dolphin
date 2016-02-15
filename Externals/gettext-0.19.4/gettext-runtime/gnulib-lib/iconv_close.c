/* Character set conversion.
   Copyright (C) 2007, 2009-2014 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <iconv.h>

#include <stdint.h>
#ifndef uintptr_t
# define uintptr_t unsigned long
#endif

int
rpl_iconv_close (iconv_t cd)
#undef iconv_close
{
#if REPLACE_ICONV_UTF
  switch ((uintptr_t) cd)
    {
    case (uintptr_t) _ICONV_UTF8_UTF16BE:
    case (uintptr_t) _ICONV_UTF8_UTF16LE:
    case (uintptr_t) _ICONV_UTF8_UTF32BE:
    case (uintptr_t) _ICONV_UTF8_UTF32LE:
    case (uintptr_t) _ICONV_UTF16BE_UTF8:
    case (uintptr_t) _ICONV_UTF16LE_UTF8:
    case (uintptr_t) _ICONV_UTF32BE_UTF8:
    case (uintptr_t) _ICONV_UTF32LE_UTF8:
      return 0;
    }
#endif
  return iconv_close (cd);
}
