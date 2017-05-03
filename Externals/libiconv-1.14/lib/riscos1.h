/*
 * Copyright (C) 1999-2002 Free Software Foundation, Inc.
 * This file is part of the GNU LIBICONV Library.
 *
 * The GNU LIBICONV Library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * The GNU LIBICONV Library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the GNU LIBICONV Library; see the file COPYING.LIB.
 * If not, write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * RISCOS-LATIN1
 */

static const unsigned short riscos1_2uni[32] = {
  /* 0x80 */
  0x221a, 0x0174, 0x0175, 0x0083, 0x2573, 0x0176, 0x0177, 0x0087,
  0x21e6, 0x21e8, 0x21e9, 0x21e7, 0x2026, 0x2122, 0x2030, 0x2022,
  /* 0x90 */
  0x2018, 0x2019, 0x2039, 0x203a, 0x201c, 0x201d, 0x201e, 0x2013,
  0x2014, 0x2212, 0x0152, 0x0153, 0x2020, 0x2021, 0xfb01, 0xfb02,
};

static int
riscos1_mbtowc (conv_t conv, ucs4_t *pwc, const unsigned char *s, int n)
{
  unsigned char c = *s;
  if (c >= 0x80 && c < 0xa0)
    *pwc = (ucs4_t) riscos1_2uni[c-0x80];
  else
    *pwc = (ucs4_t) c;
  return 1;
}

static const unsigned char riscos1_page01[40] = {
  0x00, 0x00, 0x9a, 0x9b, 0x00, 0x00, 0x00, 0x00, /* 0x50-0x57 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x58-0x5f */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60-0x67 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x68-0x6f */
  0x00, 0x00, 0x00, 0x00, 0x81, 0x82, 0x85, 0x86, /* 0x70-0x77 */
};
static const unsigned char riscos1_page20[48] = {
  0x00, 0x00, 0x00, 0x97, 0x98, 0x00, 0x00, 0x00, /* 0x10-0x17 */
  0x90, 0x91, 0x00, 0x00, 0x94, 0x95, 0x96, 0x00, /* 0x18-0x1f */
  0x9c, 0x9d, 0x8f, 0x00, 0x00, 0x00, 0x8c, 0x00, /* 0x20-0x27 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x28-0x2f */
  0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x30-0x37 */
  0x00, 0x92, 0x93, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x38-0x3f */
};
static const unsigned char riscos1_page21[16] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x8b, /* 0xe0-0xe7 */
  0x89, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe8-0xef */
};
static const unsigned char riscos1_page22[16] = {
  0x00, 0x00, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x10-0x17 */
  0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x18-0x1f */
};

static int
riscos1_wctomb (conv_t conv, unsigned char *r, ucs4_t wc, int n)
{
  unsigned char c = 0;
  if (wc < 0x0080 || wc == 0x0083 || wc == 0x0087 || (wc >= 0x00a0 && wc < 0x0100)) {
    *r = wc;
    return 1;
  }
  else if (wc >= 0x0150 && wc < 0x0178)
    c = riscos1_page01[wc-0x0150];
  else if (wc >= 0x2010 && wc < 0x2040)
    c = riscos1_page20[wc-0x2010];
  else if (wc == 0x2122)
    c = 0x8d;
  else if (wc >= 0x21e0 && wc < 0x21f0)
    c = riscos1_page21[wc-0x21e0];
  else if (wc >= 0x2210 && wc < 0x2220)
    c = riscos1_page22[wc-0x2210];
  else if (wc == 0x2573)
    c = 0x84;
  else if (wc >= 0xfb01 && wc < 0xfb03)
    c = wc-0xfa63;
  if (c != 0) {
    *r = c;
    return 1;
  }
  return RET_ILUNI;
}
