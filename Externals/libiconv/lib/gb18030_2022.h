/*
 * Copyright (C) 1999-2001, 2005, 2012, 2016, 2023 Free Software Foundation, Inc.
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
 * GB18030:2022
 */

/*
 * GB18030, in the version from 2022, is like the version from 2005,
 * except that a few mappings to the Unicode PUA have been replaced with
 * mappings to assigned Unicode characters.
 */

static int
gb18030_2022_mbtowc (conv_t conv, ucs4_t *pwc, const unsigned char *s, size_t n)
{
  int ret;

  /* Code set 0 (ASCII) */
  if (*s < 0x80)
    return ascii_mbtowc(conv,pwc,s,n);

  /* Code set 1 (GBK extended) */
  ret = gbk_mbtowc(conv,pwc,s,n);
  if (ret != RET_ILSEQ)
    return ret;

  ret = gb18030_2022_ext_mbtowc(conv,pwc,s,n);
  if (ret != RET_ILSEQ)
    return ret;

  /* Code set 2 (remainder of Unicode U+0000..U+FFFF), including
     User-defined characters, two-byte part of range U+E766..U+E864 */
  ret = gb18030_2022_uni_mbtowc(conv,pwc,s,n);
  if (ret != RET_ILSEQ)
    return ret;
  /* User-defined characters range U+E000..U+E765 */
  {
    unsigned char c1 = s[0];
    if ((c1 >= 0xaa && c1 <= 0xaf) || (c1 >= 0xf8 && c1 <= 0xfe)) {
      if (n >= 2) {
        unsigned char c2 = s[1];
        if (c2 >= 0xa1 && c2 <= 0xfe) {
          *pwc = 0xe000 + 94 * (c1 >= 0xf8 ? c1 - 0xf2 : c1 - 0xaa) + (c2 - 0xa1);
          return 2;
        }
      } else
        return RET_TOOFEW(0);
    } else if (c1 >= 0xa1 && c1 <= 0xa7) {
      if (n >= 2) {
        unsigned char c2 = s[1];
        if (c2 >= 0x40 && c2 <= 0xa1 && c2 != 0x7f) {
          *pwc = 0xe4c6 + 96 * (c1 - 0xa1) + c2 - (c2 >= 0x80 ? 0x41 : 0x40);
          return 2;
        }
      } else
        return RET_TOOFEW(0);
    }
  }

  /* Code set 3 (Unicode U+10000..U+10FFFF) */
  {
    unsigned char c1 = s[0];
    if (c1 >= 0x90 && c1 <= 0xe3) {
      if (n >= 2) {
        unsigned char c2 = s[1];
        if (c2 >= 0x30 && c2 <= 0x39) {
          if (n >= 3) {
            unsigned char c3 = s[2];
            if (c3 >= 0x81 && c3 <= 0xfe) {
              if (n >= 4) {
                unsigned char c4 = s[3];
                if (c4 >= 0x30 && c4 <= 0x39) {
                  unsigned int i = (((c1 - 0x90) * 10 + (c2 - 0x30)) * 126 + (c3 - 0x81)) * 10 + (c4 - 0x30);
                  if (i >= 0 && i < 0x100000) {
                    *pwc = (ucs4_t) (0x10000 + i);
                    return 4;
                  }
                }
                return RET_ILSEQ;
              }
              return RET_TOOFEW(0);
            }
            return RET_ILSEQ;
          }
          return RET_TOOFEW(0);
        }
        return RET_ILSEQ;
      }
      return RET_TOOFEW(0);
    }
    return RET_ILSEQ;
  }
}

static const unsigned short gb18030_2022_pua2charset[23*3] = {
/* Unicode range   GB18030 range */
  0xe766, 0xe76b,  0xa2ab, /*.. 0xa2b0, */
  0xe76d, 0xe76d,  0xa2e4,
  0xe76e, 0xe76f,  0xa2ef, /*.. 0xa2f0, */
  0xe770, 0xe771,  0xa2fd, /*.. 0xa2fe, */
  0xe772, 0xe77c,  0xa4f4, /*.. 0xa4fe, */
  0xe77d, 0xe784,  0xa5f7, /*.. 0xa5fe, */
  0xe785, 0xe78c,  0xa6b9, /*.. 0xa6c0, */
  0xe797, 0xe79f,  0xa6f6, /*.. 0xa6fe, */
  0xe7a0, 0xe7ae,  0xa7c2, /*.. 0xa7d0, */
  0xe7af, 0xe7bb,  0xa7f2, /*.. 0xa7fe, */
  0xe7bc, 0xe7c6,  0xa896, /*.. 0xa8a0, */
  0xe7c9, 0xe7cc,  0xa8c1, /*.. 0xa8c4, */
  0xe7cd, 0xe7e1,  0xa8ea, /*.. 0xa8fe, */
  0xe7e2, 0xe7e2,  0xa958,
  0xe7e3, 0xe7e3,  0xa95b,
  0xe7e4, 0xe7e6,  0xa95d, /*.. 0xa95f, */
  0xe7f4, 0xe800,  0xa997, /*.. 0xa9a3, */
  0xe801, 0xe80f,  0xa9f0, /*.. 0xa9fe, */
  0xe810, 0xe814,  0xd7fa, /*.. 0xd7fe, */
  0xe816, 0xe818,  0xfe51, /*.. 0xfe53, */
  0xe831, 0xe831,  0xfe6c,
  0xe83b, 0xe83b,  0xfe76,
  0xe855, 0xe855,  0xfe91,
};

static int
gb18030_2022_wctomb (conv_t conv, unsigned char *r, ucs4_t wc, size_t n)
{
  int ret;

  /* Code set 0 (ASCII) */
  ret = ascii_wctomb(conv,r,wc,n);
  if (ret != RET_ILUNI)
    return ret;

  /* Code set 1 (GBK extended) */
  ret = gbk_wctomb(conv,r,wc,n);
  if (ret != RET_ILUNI)
    return ret;

  ret = gb18030ext_wctomb(conv,r,wc,n);
  if (ret != RET_ILUNI)
    return ret;

  /* Code set 2 (remainder of Unicode U+0000..U+FFFF) */
  if (wc >= 0xe000 && wc <= 0xe864) {
    if (n >= 2) {
      if (wc < 0xe766) {
        /* User-defined characters range U+E000..U+E765 */
        if (wc < 0xe4c6) {
          unsigned int i = wc - 0xe000;
          r[1] = (i % 94) + 0xa1; i = i / 94;
          r[0] = (i < 6 ? i + 0xaa : i + 0xf2);
          return 2;
        } else {
          unsigned int i = wc - 0xe4c6;
          r[0] = (i / 96) + 0xa1; i = i % 96;
          r[1] = i + (i >= 0x3f ? 0x41 : 0x40);
          return 2;
        }
      } else {
        /* User-defined characters, two-byte part of range U+E766..U+E864 */
        unsigned int k1 = 0;
        unsigned int k2 = 23;
        /* Invariant: We know that if wc occurs in Unicode interval in
           gb18030_2022_pua2charset, it does so at a k with  k1 <= k < k2. */
        while (k1 < k2) {
          unsigned int k = (k1 + k2) / 2;
          if (wc < gb18030_2022_pua2charset[k*3+0])
            k2 = k;
          else if (wc > gb18030_2022_pua2charset[k*3+1])
            k1 = k + 1;
          else {
            unsigned short c =
              gb18030_2022_pua2charset[k*3+2] + (wc - gb18030_2022_pua2charset[k*3+0]);
            r[0] = (c >> 8);
            r[1] = (c & 0xff);
            return 2;
          }
        }
      }
    } else
      return RET_TOOSMALL;
  }
  ret = gb18030_2022_uni_wctomb(conv,r,wc,n);
  if (ret != RET_ILUNI)
    return ret;

  /* Code set 3 (Unicode U+10000..U+10FFFF) */
  if (n >= 4) {
    if (wc >= 0x10000 && wc < 0x110000) {
      unsigned int i = wc - 0x10000;
      r[3] = (i % 10) + 0x30; i = i / 10;
      r[2] = (i % 126) + 0x81; i = i / 126;
      r[1] = (i % 10) + 0x30; i = i / 10;
      r[0] = i + 0x90;
      return 4;
    }
    return RET_ILUNI;
  }
  return RET_TOOSMALL;
}
