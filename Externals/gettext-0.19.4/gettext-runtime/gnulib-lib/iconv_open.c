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

#include <errno.h>
#include <string.h>
#include "c-ctype.h"
#include "c-strcase.h"

#define SIZEOF(a) (sizeof(a) / sizeof(a[0]))

/* Namespace cleanliness.  */
#define mapping_lookup rpl_iconv_open_mapping_lookup

/* The macro ICONV_FLAVOR is defined to one of these or undefined.  */

#define ICONV_FLAVOR_AIX "iconv_open-aix.h"
#define ICONV_FLAVOR_HPUX "iconv_open-hpux.h"
#define ICONV_FLAVOR_IRIX "iconv_open-irix.h"
#define ICONV_FLAVOR_OSF "iconv_open-osf.h"
#define ICONV_FLAVOR_SOLARIS "iconv_open-solaris.h"

#ifdef ICONV_FLAVOR
# include ICONV_FLAVOR
#endif

iconv_t
rpl_iconv_open (const char *tocode, const char *fromcode)
#undef iconv_open
{
  char fromcode_upper[32];
  char tocode_upper[32];
  char *fromcode_upper_end;
  char *tocode_upper_end;

#if REPLACE_ICONV_UTF
  /* Special handling of conversion between UTF-8 and UTF-{16,32}{BE,LE}.
     Do this here, before calling the real iconv_open(), because  OSF/1 5.1
     iconv() to these encoding inserts a BOM, which is wrong.
     We do not need to handle conversion between arbitrary encodings and
     UTF-{16,32}{BE,LE}, because the 'striconveh' module implements two-step
     conversion through UTF-8.
     The _ICONV_* constants are chosen to be disjoint from any iconv_t
     returned by the system's iconv_open() functions.  Recall that iconv_t
     is a scalar type.  */
  if (c_toupper (fromcode[0]) == 'U'
      && c_toupper (fromcode[1]) == 'T'
      && c_toupper (fromcode[2]) == 'F'
      && fromcode[3] == '-')
    {
      if (c_toupper (tocode[0]) == 'U'
          && c_toupper (tocode[1]) == 'T'
          && c_toupper (tocode[2]) == 'F'
          && tocode[3] == '-')
        {
          if (strcmp (fromcode + 4, "8") == 0)
            {
              if (c_strcasecmp (tocode + 4, "16BE") == 0)
                return _ICONV_UTF8_UTF16BE;
              if (c_strcasecmp (tocode + 4, "16LE") == 0)
                return _ICONV_UTF8_UTF16LE;
              if (c_strcasecmp (tocode + 4, "32BE") == 0)
                return _ICONV_UTF8_UTF32BE;
              if (c_strcasecmp (tocode + 4, "32LE") == 0)
                return _ICONV_UTF8_UTF32LE;
            }
          else if (strcmp (tocode + 4, "8") == 0)
            {
              if (c_strcasecmp (fromcode + 4, "16BE") == 0)
                return _ICONV_UTF16BE_UTF8;
              if (c_strcasecmp (fromcode + 4, "16LE") == 0)
                return _ICONV_UTF16LE_UTF8;
              if (c_strcasecmp (fromcode + 4, "32BE") == 0)
                return _ICONV_UTF32BE_UTF8;
              if (c_strcasecmp (fromcode + 4, "32LE") == 0)
                return _ICONV_UTF32LE_UTF8;
            }
        }
    }
#endif

  /* Do *not* add special support for 8-bit encodings like ASCII or ISO-8859-1
     here.  This would lead to programs that work in some locales (such as the
     "C" or "en_US" locales) but do not work in East Asian locales.  It is
     better if programmers make their programs depend on GNU libiconv (except
     on glibc systems), e.g. by using the AM_ICONV macro and documenting the
     dependency in an INSTALL or DEPENDENCIES file.  */

  /* Try with the original names first.
     This covers the case when fromcode or tocode is a lowercase encoding name
     that is understood by the system's iconv_open but not listed in our
     mappings table.  */
  {
    iconv_t cd = iconv_open (tocode, fromcode);
    if (cd != (iconv_t)(-1))
      return cd;
  }

  /* Convert the encodings to upper case, because
       1. in the arguments of iconv_open() on AIX, HP-UX, and OSF/1 the case
          matters,
       2. it makes searching in the table faster.  */
  {
    const char *p = fromcode;
    char *q = fromcode_upper;
    while ((*q = c_toupper (*p)) != '\0')
      {
        p++;
        q++;
        if (q == &fromcode_upper[SIZEOF (fromcode_upper)])
          {
            errno = EINVAL;
            return (iconv_t)(-1);
          }
      }
    fromcode_upper_end = q;
  }

  {
    const char *p = tocode;
    char *q = tocode_upper;
    while ((*q = c_toupper (*p)) != '\0')
      {
        p++;
        q++;
        if (q == &tocode_upper[SIZEOF (tocode_upper)])
          {
            errno = EINVAL;
            return (iconv_t)(-1);
          }
      }
    tocode_upper_end = q;
  }

#ifdef ICONV_FLAVOR
  /* Apply the mappings.  */
  {
    const struct mapping *m =
      mapping_lookup (fromcode_upper, fromcode_upper_end - fromcode_upper);

    fromcode = (m != NULL ? m->vendor_name : fromcode_upper);
  }
  {
    const struct mapping *m =
      mapping_lookup (tocode_upper, tocode_upper_end - tocode_upper);

    tocode = (m != NULL ? m->vendor_name : tocode_upper);
  }
#else
  fromcode = fromcode_upper;
  tocode = tocode_upper;
#endif

  return iconv_open (tocode, fromcode);
}
