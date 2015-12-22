/* Character set conversion.
   Copyright (C) 1999-2001, 2007, 2009-2014 Free Software Foundation, Inc.

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

#include <stddef.h>

#if REPLACE_ICONV_UTF
# include <errno.h>
# include <stdint.h>
# include <stdlib.h>
# include "unistr.h"
# ifndef uintptr_t
#  define uintptr_t unsigned long
# endif
#endif

#if REPLACE_ICONV_UTF

/* UTF-{16,32}{BE,LE} converters taken from GNU libiconv 1.11.  */

/* Return code if invalid. (xxx_mbtowc) */
# define RET_ILSEQ      -1
/* Return code if no bytes were read. (xxx_mbtowc) */
# define RET_TOOFEW     -2

/* Return code if invalid. (xxx_wctomb) */
# define RET_ILUNI      -1
/* Return code if output buffer is too small. (xxx_wctomb, xxx_reset) */
# define RET_TOOSMALL   -2

/*
 * UTF-16BE
 */

/* Specification: RFC 2781 */

static int
utf16be_mbtowc (ucs4_t *pwc, const unsigned char *s, size_t n)
{
  if (n >= 2)
    {
      ucs4_t wc = (s[0] << 8) + s[1];
      if (wc >= 0xd800 && wc < 0xdc00)
        {
          if (n >= 4)
            {
              ucs4_t wc2 = (s[2] << 8) + s[3];
              if (!(wc2 >= 0xdc00 && wc2 < 0xe000))
                return RET_ILSEQ;
              *pwc = 0x10000 + ((wc - 0xd800) << 10) + (wc2 - 0xdc00);
              return 4;
            }
        }
      else if (wc >= 0xdc00 && wc < 0xe000)
        {
          return RET_ILSEQ;
        }
      else
        {
          *pwc = wc;
          return 2;
        }
    }
  return RET_TOOFEW;
}

static int
utf16be_wctomb (unsigned char *r, ucs4_t wc, size_t n)
{
  if (!(wc >= 0xd800 && wc < 0xe000))
    {
      if (wc < 0x10000)
        {
          if (n >= 2)
            {
              r[0] = (unsigned char) (wc >> 8);
              r[1] = (unsigned char) wc;
              return 2;
            }
          else
            return RET_TOOSMALL;
        }
      else if (wc < 0x110000)
        {
          if (n >= 4)
            {
              ucs4_t wc1 = 0xd800 + ((wc - 0x10000) >> 10);
              ucs4_t wc2 = 0xdc00 + ((wc - 0x10000) & 0x3ff);
              r[0] = (unsigned char) (wc1 >> 8);
              r[1] = (unsigned char) wc1;
              r[2] = (unsigned char) (wc2 >> 8);
              r[3] = (unsigned char) wc2;
              return 4;
            }
          else
            return RET_TOOSMALL;
        }
    }
  return RET_ILUNI;
}

/*
 * UTF-16LE
 */

/* Specification: RFC 2781 */

static int
utf16le_mbtowc (ucs4_t *pwc, const unsigned char *s, size_t n)
{
  if (n >= 2)
    {
      ucs4_t wc = s[0] + (s[1] << 8);
      if (wc >= 0xd800 && wc < 0xdc00)
        {
          if (n >= 4)
            {
              ucs4_t wc2 = s[2] + (s[3] << 8);
              if (!(wc2 >= 0xdc00 && wc2 < 0xe000))
                return RET_ILSEQ;
              *pwc = 0x10000 + ((wc - 0xd800) << 10) + (wc2 - 0xdc00);
              return 4;
            }
        }
      else if (wc >= 0xdc00 && wc < 0xe000)
        {
          return RET_ILSEQ;
        }
      else
        {
          *pwc = wc;
          return 2;
        }
    }
  return RET_TOOFEW;
}

static int
utf16le_wctomb (unsigned char *r, ucs4_t wc, size_t n)
{
  if (!(wc >= 0xd800 && wc < 0xe000))
    {
      if (wc < 0x10000)
        {
          if (n >= 2)
            {
              r[0] = (unsigned char) wc;
              r[1] = (unsigned char) (wc >> 8);
              return 2;
            }
          else
            return RET_TOOSMALL;
        }
      else if (wc < 0x110000)
        {
          if (n >= 4)
            {
              ucs4_t wc1 = 0xd800 + ((wc - 0x10000) >> 10);
              ucs4_t wc2 = 0xdc00 + ((wc - 0x10000) & 0x3ff);
              r[0] = (unsigned char) wc1;
              r[1] = (unsigned char) (wc1 >> 8);
              r[2] = (unsigned char) wc2;
              r[3] = (unsigned char) (wc2 >> 8);
              return 4;
            }
          else
            return RET_TOOSMALL;
        }
    }
  return RET_ILUNI;
}

/*
 * UTF-32BE
 */

/* Specification: Unicode 3.1 Standard Annex #19 */

static int
utf32be_mbtowc (ucs4_t *pwc, const unsigned char *s, size_t n)
{
  if (n >= 4)
    {
      ucs4_t wc = (s[0] << 24) + (s[1] << 16) + (s[2] << 8) + s[3];
      if (wc < 0x110000 && !(wc >= 0xd800 && wc < 0xe000))
        {
          *pwc = wc;
          return 4;
        }
      else
        return RET_ILSEQ;
    }
  return RET_TOOFEW;
}

static int
utf32be_wctomb (unsigned char *r, ucs4_t wc, size_t n)
{
  if (wc < 0x110000 && !(wc >= 0xd800 && wc < 0xe000))
    {
      if (n >= 4)
        {
          r[0] = 0;
          r[1] = (unsigned char) (wc >> 16);
          r[2] = (unsigned char) (wc >> 8);
          r[3] = (unsigned char) wc;
          return 4;
        }
      else
        return RET_TOOSMALL;
    }
  return RET_ILUNI;
}

/*
 * UTF-32LE
 */

/* Specification: Unicode 3.1 Standard Annex #19 */

static int
utf32le_mbtowc (ucs4_t *pwc, const unsigned char *s, size_t n)
{
  if (n >= 4)
    {
      ucs4_t wc = s[0] + (s[1] << 8) + (s[2] << 16) + (s[3] << 24);
      if (wc < 0x110000 && !(wc >= 0xd800 && wc < 0xe000))
        {
          *pwc = wc;
          return 4;
        }
      else
        return RET_ILSEQ;
    }
  return RET_TOOFEW;
}

static int
utf32le_wctomb (unsigned char *r, ucs4_t wc, size_t n)
{
  if (wc < 0x110000 && !(wc >= 0xd800 && wc < 0xe000))
    {
      if (n >= 4)
        {
          r[0] = (unsigned char) wc;
          r[1] = (unsigned char) (wc >> 8);
          r[2] = (unsigned char) (wc >> 16);
          r[3] = 0;
          return 4;
        }
      else
        return RET_TOOSMALL;
    }
  return RET_ILUNI;
}

#endif

size_t
rpl_iconv (iconv_t cd,
           ICONV_CONST char **inbuf, size_t *inbytesleft,
           char **outbuf, size_t *outbytesleft)
#undef iconv
{
#if REPLACE_ICONV_UTF
  switch ((uintptr_t) cd)
    {
      {
        int (*xxx_wctomb) (unsigned char *, ucs4_t, size_t);

        case (uintptr_t) _ICONV_UTF8_UTF16BE:
          xxx_wctomb = utf16be_wctomb;
          goto loop_from_utf8;
        case (uintptr_t) _ICONV_UTF8_UTF16LE:
          xxx_wctomb = utf16le_wctomb;
          goto loop_from_utf8;
        case (uintptr_t) _ICONV_UTF8_UTF32BE:
          xxx_wctomb = utf32be_wctomb;
          goto loop_from_utf8;
        case (uintptr_t) _ICONV_UTF8_UTF32LE:
          xxx_wctomb = utf32le_wctomb;
          goto loop_from_utf8;

       loop_from_utf8:
        if (inbuf == NULL || *inbuf == NULL)
          return 0;
        {
          ICONV_CONST char *inptr = *inbuf;
          size_t inleft = *inbytesleft;
          char *outptr = *outbuf;
          size_t outleft = *outbytesleft;
          size_t res = 0;
          while (inleft > 0)
            {
              ucs4_t uc;
              int m = u8_mbtoucr (&uc, (const uint8_t *) inptr, inleft);
              if (m <= 0)
                {
                  if (m == -1)
                    {
                      errno = EILSEQ;
                      res = (size_t)(-1);
                      break;
                    }
                  if (m == -2)
                    {
                      errno = EINVAL;
                      res = (size_t)(-1);
                      break;
                    }
                  abort ();
                }
              else
                {
                  int n = xxx_wctomb ((uint8_t *) outptr, uc, outleft);
                  if (n < 0)
                    {
                      if (n == RET_ILUNI)
                        {
                          errno = EILSEQ;
                          res = (size_t)(-1);
                          break;
                        }
                      if (n == RET_TOOSMALL)
                        {
                          errno = E2BIG;
                          res = (size_t)(-1);
                          break;
                        }
                      abort ();
                    }
                  else
                    {
                      inptr += m;
                      inleft -= m;
                      outptr += n;
                      outleft -= n;
                    }
                }
            }
          *inbuf = inptr;
          *inbytesleft = inleft;
          *outbuf = outptr;
          *outbytesleft = outleft;
          return res;
        }
      }

      {
        int (*xxx_mbtowc) (ucs4_t *, const unsigned char *, size_t);

        case (uintptr_t) _ICONV_UTF16BE_UTF8:
          xxx_mbtowc = utf16be_mbtowc;
          goto loop_to_utf8;
        case (uintptr_t) _ICONV_UTF16LE_UTF8:
          xxx_mbtowc = utf16le_mbtowc;
          goto loop_to_utf8;
        case (uintptr_t) _ICONV_UTF32BE_UTF8:
          xxx_mbtowc = utf32be_mbtowc;
          goto loop_to_utf8;
        case (uintptr_t) _ICONV_UTF32LE_UTF8:
          xxx_mbtowc = utf32le_mbtowc;
          goto loop_to_utf8;

       loop_to_utf8:
        if (inbuf == NULL || *inbuf == NULL)
          return 0;
        {
          ICONV_CONST char *inptr = *inbuf;
          size_t inleft = *inbytesleft;
          char *outptr = *outbuf;
          size_t outleft = *outbytesleft;
          size_t res = 0;
          while (inleft > 0)
            {
              ucs4_t uc;
              int m = xxx_mbtowc (&uc, (const uint8_t *) inptr, inleft);
              if (m <= 0)
                {
                  if (m == RET_ILSEQ)
                    {
                      errno = EILSEQ;
                      res = (size_t)(-1);
                      break;
                    }
                  if (m == RET_TOOFEW)
                    {
                      errno = EINVAL;
                      res = (size_t)(-1);
                      break;
                    }
                  abort ();
                }
              else
                {
                  int n = u8_uctomb ((uint8_t *) outptr, uc, outleft);
                  if (n < 0)
                    {
                      if (n == -1)
                        {
                          errno = EILSEQ;
                          res = (size_t)(-1);
                          break;
                        }
                      if (n == -2)
                        {
                          errno = E2BIG;
                          res = (size_t)(-1);
                          break;
                        }
                      abort ();
                    }
                  else
                    {
                      inptr += m;
                      inleft -= m;
                      outptr += n;
                      outleft -= n;
                    }
                }
            }
          *inbuf = inptr;
          *inbytesleft = inleft;
          *outbuf = outptr;
          *outbytesleft = outleft;
          return res;
        }
      }
    }
#endif
  return iconv (cd, inbuf, inbytesleft, outbuf, outbytesleft);
}
