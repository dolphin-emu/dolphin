/*
 * Copyright (C) 1999-2000, 2016 Free Software Foundation, Inc.
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
 * UCS-4-INTERNAL = UCS-4 with machine dependent endianness and alignment
 */

static int
ucs4internal_mbtowc (conv_t conv, ucs4_t *pwc, const unsigned char *s, size_t n)
{
  if (n >= 4) {
    *pwc = *(const unsigned int *)s;
    return 4;
  }
  return RET_TOOFEW(0);
}

static int
ucs4internal_wctomb (conv_t conv, unsigned char *r, ucs4_t wc, size_t n)
{
  if (n >= 4) {
    *(unsigned int *)r = wc;
    return 4;
  } else
    return RET_TOOSMALL;
}
