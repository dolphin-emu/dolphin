/*
 * Copyright (C) 1999-2000, 2016, 2024 Free Software Foundation, Inc.
 * This file is part of the GNU LIBICONV Library.
 *
 * The GNU LIBICONV Library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * The GNU LIBICONV Library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the GNU LIBICONV Library; see the file COPYING.LIB.
 * If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * UCS-4BE = UCS-4 big endian
 */

static int
ucs4be_mbtowc (conv_t conv, ucs4_t *pwc, const unsigned char *s, size_t n)
{
  if (n >= 4) {
    *pwc =   ((ucs4_t) s[0] << 24)
           + ((ucs4_t) s[1] << 16)
           + ((ucs4_t) s[2] << 8)
           +  (ucs4_t) s[3];
    return 4;
  }
  return RET_TOOFEW(0);
}

static int
ucs4be_wctomb (conv_t conv, unsigned char *r, ucs4_t wc, size_t n)
{
  if (n >= 4) {
    r[0] = (unsigned char) (wc >> 24);
    r[1] = (unsigned char) (wc >> 16);
    r[2] = (unsigned char) (wc >> 8);
    r[3] = (unsigned char) wc;
    return 4;
  } else
    return RET_TOOSMALL;
}
