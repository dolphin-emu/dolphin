/*
 * Copyright (C) 1999-2010, 2012, 2016 Free Software Foundation, Inc.
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
 * HKSCS:2008
 */

static const unsigned short hkscs2008_2uni_page87[126] = {
  /* 0x87 */
  0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd,
  0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd,
  0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd,
  0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd,
  0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd,
  0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd,
  0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd, 0x22fd,
  0x22fd, 0x22fd, 0x0175, 0x2453, 0x299e, 0x2d21, 0x03ec, 0x2bde,
  0x02f5, 0x16fc, 0x2197, 0x2a61, 0x300d, 0x26ea, 0x238a, 0x275e,
  0x060a, 0x1884, 0x2196, 0x1f2f, 0x0930, 0x1a13, 0x0d96, 0x204a,
  0x1e18, 0x15d0, 0x1632, 0x0f60, 0x1129, 0x1b9d, 0x144c, 0x17c5,
  0x1082, 0x162c, 0x0a4f, 0x1d46, 0x00e6, 0x13c4, 0x2cb9, 0x14c6,
  0x21c7, 0x0cb3, 0x092f, 0x0b4c, 0x0531, 0x298e, 0x0d18, 0x1672,
  0x2f65, 0x1c8f, 0x08ae, 0x2e88, 0x0581, 0x2c99, 0x17ae, 0x25bc,
  0x21c8, 0x25c1, 0x25c9, 0x25cc, 0x21c9, 0x1904, 0x28bb, 0x04b4,
  0x21ca, 0x07e1, 0x31ff, 0x0ec1, 0x126e, 0x21cb,
};

static const ucs4_t hkscs2008_2uni_upages[50] = {
  0x03400, 0x03800, 0x03a00, 0x03e00, 0x04000, 0x04100, 0x04300, 0x04400,
  0x04600, 0x04900, 0x05200, 0x05400, 0x05700, 0x05800, 0x06200, 0x06600,
  0x06700, 0x06a00, 0x07000, 0x07300, 0x07400, 0x07900, 0x07a00, 0x07b00,
  0x08400, 0x08500, 0x08600, 0x08800, 0x08b00, 0x09000, 0x09200, 0x09400,
  0x09700, 0x09f00, 0x0ff00, 0x20a00, 0x21d00, 0x22400, 0x23100, 0x23200,
  0x23500, 0x23600, 0x24100, 0x25800, 0x25d00, 0x26000, 0x26e00, 0x27b00,
  0x28900, 0x2ad00,
};

static int
hkscs2008_mbtowc (conv_t conv, ucs4_t *pwc, const unsigned char *s, size_t n)
{
  unsigned char c1 = s[0];
  if ((c1 == 0x87)) {
    if (n >= 2) {
      unsigned char c2 = s[1];
      if ((c2 >= 0x40 && c2 < 0x7f) || (c2 >= 0xa1 && c2 < 0xff)) {
        unsigned int i = 157 * (c1 - 0x80) + (c2 - (c2 >= 0xa1 ? 0x62 : 0x40));
        ucs4_t wc = 0xfffd;
        unsigned short swc;
        {
          if (i < 1225)
            swc = hkscs2008_2uni_page87[i-1099],
            wc = hkscs2008_2uni_upages[swc>>8] | (swc & 0xff);
        }
        if (wc != 0xfffd) {
          *pwc = wc;
          return 2;
        }
      }
      return RET_ILSEQ;
    }
    return RET_TOOFEW(0);
  }
  return RET_ILSEQ;
}

static const unsigned short hkscs2008_2charset[68] = {
  0x87be, 0x877a, 0x87a2, 0x877e, 0x87d9, 0x87c6, 0x87ce, 0x87aa,
  0x87db, 0x87cc, 0x87c4, 0x87ae, 0x87bc, 0x87c5, 0x87c3, 0x87c8,
  0x87b0, 0x87dd, 0x87b5, 0x87ba, 0x87b6, 0x87de, 0x87bf, 0x87b8,
  0x87c1, 0x87b3, 0x87bb, 0x87b4, 0x87c9, 0x87a3, 0x87d0, 0x87b9,
  0x87ab, 0x87d7, 0x87af, 0x87b7, 0x87cb, 0x87bd, 0x87b2, 0x87ad,
  0x87b1, 0x87ac, 0x87a4, 0x87c2, 0x87d2, 0x87d6, 0x87da, 0x87df,
  0x87a8, 0x877b, 0x87d1, 0x87d3, 0x87d4, 0x87d5, 0x87a7, 0x87a9,
  0x87d8, 0x87c7, 0x877c, 0x87a5, 0x87a1, 0x87cf, 0x87c0, 0x877d,
  0x87cd, 0x87ca, 0x87a6, 0x87dc,
};

static const Summary16 hkscs2008_uni2indx_page34[15] = {
  /* 0x3400 */
  {    0, 0x0000 }, {    0, 0x0000 }, {    0, 0x0000 }, {    0, 0x0000 },
  {    0, 0x0000 }, {    0, 0x0000 }, {    0, 0x0000 }, {    0, 0x0000 },
  {    0, 0x0000 }, {    0, 0x0000 }, {    0, 0x0000 }, {    0, 0x0000 },
  {    0, 0x0000 }, {    0, 0x0000 }, {    0, 0x0040 },
};
static const Summary16 hkscs2008_uni2indx_page38[8] = {
  /* 0x3800 */
  {    1, 0x0000 }, {    1, 0x0000 }, {    1, 0x0000 }, {    1, 0x0000 },
  {    1, 0x0000 }, {    1, 0x0000 }, {    1, 0x0000 }, {    1, 0x0020 },
};
static const Summary16 hkscs2008_uni2indx_page3a[16] = {
  /* 0x3a00 */
  {    2, 0x0000 }, {    2, 0x0000 }, {    2, 0x0000 }, {    2, 0x0000 },
  {    2, 0x0000 }, {    2, 0x0000 }, {    2, 0x0000 }, {    2, 0x0000 },
  {    2, 0x0000 }, {    2, 0x0000 }, {    2, 0x0000 }, {    2, 0x0000 },
  {    2, 0x0000 }, {    2, 0x0000 }, {    2, 0x0000 }, {    2, 0x0020 },
};
static const Summary16 hkscs2008_uni2indx_page3e[15] = {
  /* 0x3e00 */
  {    3, 0x0000 }, {    3, 0x0000 }, {    3, 0x0000 }, {    3, 0x0000 },
  {    3, 0x0000 }, {    3, 0x0000 }, {    3, 0x0000 }, {    3, 0x0000 },
  {    3, 0x0000 }, {    3, 0x0000 }, {    3, 0x0000 }, {    3, 0x0000 },
  {    3, 0x0000 }, {    3, 0x0000 }, {    3, 0x1000 },
};
static const Summary16 hkscs2008_uni2indx_page40[25] = {
  /* 0x4000 */
  {    4, 0x0000 }, {    4, 0x0000 }, {    4, 0x0000 }, {    4, 0x0000 },
  {    4, 0x0000 }, {    4, 0x0000 }, {    4, 0x0000 }, {    4, 0x0000 },
  {    4, 0x0000 }, {    4, 0x0000 }, {    4, 0x0000 }, {    4, 0x0010 },
  {    5, 0x0000 }, {    5, 0x0000 }, {    5, 0x0000 }, {    5, 0x0000 },
  /* 0x4100 */
  {    5, 0x0000 }, {    5, 0x0000 }, {    5, 0x0000 }, {    5, 0x0002 },
  {    6, 0x0000 }, {    6, 0x0000 }, {    6, 0x0000 }, {    6, 0x0000 },
  {    6, 0x0002 },
};
static const Summary16 hkscs2008_uni2indx_page43[31] = {
  /* 0x4300 */
  {    7, 0x0400 }, {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 },
  {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 },
  {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 },
  {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 },
  /* 0x4400 */
  {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 },
  {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 },
  {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0000 },
  {    8, 0x0000 }, {    8, 0x0000 }, {    8, 0x0002 },
};
static const Summary16 hkscs2008_uni2indx_page46[11] = {
  /* 0x4600 */
  {    9, 0x0000 }, {    9, 0x0000 }, {    9, 0x0000 }, {    9, 0x0000 },
  {    9, 0x0000 }, {    9, 0x0000 }, {    9, 0x0000 }, {    9, 0x0000 },
  {    9, 0x0000 }, {    9, 0x0000 }, {    9, 0x4000 },
};
static const Summary16 hkscs2008_uni2indx_page49[4] = {
  /* 0x4900 */
  {   10, 0x0000 }, {   10, 0x0000 }, {   10, 0x8000 }, {   11, 0x0001 },
};
static const Summary16 hkscs2008_uni2indx_page52[5] = {
  /* 0x5200 */
  {   12, 0x0000 }, {   12, 0x0000 }, {   12, 0x0000 }, {   12, 0x0000 },
  {   12, 0x8000 },
};
static const Summary16 hkscs2008_uni2indx_page54[5] = {
  /* 0x5400 */
  {   13, 0x0000 }, {   13, 0x0000 }, {   13, 0x0000 }, {   13, 0x0000 },
  {   13, 0x1000 },
};
static const Summary16 hkscs2008_uni2indx_page57[26] = {
  /* 0x5700 */
  {   14, 0x0000 }, {   14, 0x0000 }, {   14, 0x0000 }, {   14, 0x0000 },
  {   14, 0x0000 }, {   14, 0x0000 }, {   14, 0x0000 }, {   14, 0x0000 },
  {   14, 0x0000 }, {   14, 0x0000 }, {   14, 0x0000 }, {   14, 0x0008 },
  {   15, 0x0000 }, {   15, 0x0000 }, {   15, 0x0000 }, {   15, 0x0000 },
  /* 0x5800 */
  {   15, 0x0000 }, {   15, 0x0100 }, {   16, 0x0000 }, {   16, 0x0000 },
  {   16, 0x0000 }, {   16, 0x0000 }, {   16, 0x0000 }, {   16, 0x0000 },
  {   16, 0x0000 }, {   16, 0x0040 },
};
static const Summary16 hkscs2008_uni2indx_page62[13] = {
  /* 0x6200 */
  {   17, 0x0000 }, {   17, 0x0000 }, {   17, 0x0000 }, {   17, 0x0000 },
  {   17, 0x0000 }, {   17, 0x0000 }, {   17, 0x0000 }, {   17, 0x0000 },
  {   17, 0x0000 }, {   17, 0x0000 }, {   17, 0x0000 }, {   17, 0x0000 },
  {   17, 0x0002 },
};
static const Summary16 hkscs2008_uni2indx_page66[25] = {
  /* 0x6600 */
  {   18, 0x0000 }, {   18, 0x0000 }, {   18, 0x0000 }, {   18, 0x0000 },
  {   18, 0x0000 }, {   18, 0x0000 }, {   18, 0x0001 }, {   19, 0x0000 },
  {   19, 0x0000 }, {   19, 0x0000 }, {   19, 0x0000 }, {   19, 0x0000 },
  {   19, 0x0000 }, {   19, 0x0000 }, {   19, 0x0000 }, {   19, 0x0000 },
  /* 0x6700 */
  {   19, 0x0000 }, {   19, 0x0000 }, {   19, 0x0000 }, {   19, 0x0000 },
  {   19, 0x0000 }, {   19, 0x0000 }, {   19, 0x0000 }, {   19, 0x0000 },
  {   19, 0x0004 },
};
static const Summary16 hkscs2008_uni2indx_page6a[3] = {
  /* 0x6a00 */
  {   20, 0x0000 }, {   20, 0x0000 }, {   20, 0x0200 },
};
static const Summary16 hkscs2008_uni2indx_page70[7] = {
  /* 0x7000 */
  {   21, 0x0000 }, {   21, 0x0000 }, {   21, 0x0000 }, {   21, 0x0000 },
  {   21, 0x0000 }, {   21, 0x0000 }, {   21, 0x4000 },
};
static const Summary16 hkscs2008_uni2indx_page73[29] = {
  /* 0x7300 */
  {   22, 0x0000 }, {   22, 0x0000 }, {   22, 0x0000 }, {   22, 0x0000 },
  {   22, 0x0000 }, {   22, 0x0000 }, {   22, 0x0000 }, {   22, 0x0000 },
  {   22, 0x0000 }, {   22, 0x0000 }, {   22, 0x0000 }, {   22, 0x0000 },
  {   22, 0x0010 }, {   23, 0x0000 }, {   23, 0x0000 }, {   23, 0x0000 },
  /* 0x7400 */
  {   23, 0x0000 }, {   23, 0x0000 }, {   23, 0x0000 }, {   23, 0x0000 },
  {   23, 0x1000 }, {   24, 0x0000 }, {   24, 0x0000 }, {   24, 0x0000 },
  {   24, 0x0000 }, {   24, 0x0000 }, {   24, 0x0000 }, {   24, 0x0000 },
  {   24, 0x0040 },
};
static const Summary16 hkscs2008_uni2indx_page79[45] = {
  /* 0x7900 */
  {   25, 0x0000 }, {   25, 0x0000 }, {   25, 0x0000 }, {   25, 0x0000 },
  {   25, 0x0000 }, {   25, 0x0000 }, {   25, 0x0000 }, {   25, 0x0000 },
  {   25, 0x0000 }, {   25, 0x0000 }, {   25, 0x0000 }, {   25, 0x0000 },
  {   25, 0x0000 }, {   25, 0x0001 }, {   26, 0x0000 }, {   26, 0x0000 },
  /* 0x7a00 */
  {   26, 0x0000 }, {   26, 0x0000 }, {   26, 0x1000 }, {   27, 0x0004 },
  {   28, 0x0000 }, {   28, 0x0000 }, {   28, 0x0000 }, {   28, 0x0004 },
  {   29, 0x0000 }, {   29, 0x0000 }, {   29, 0x0000 }, {   29, 0x0000 },
  {   29, 0x0000 }, {   29, 0x0000 }, {   29, 0x0000 }, {   29, 0x1000 },
  /* 0x7b00 */
  {   30, 0x0000 }, {   30, 0x0000 }, {   30, 0x0000 }, {   30, 0x0000 },
  {   30, 0x0000 }, {   30, 0x0000 }, {   30, 0x0000 }, {   30, 0x0000 },
  {   30, 0x0000 }, {   30, 0x0000 }, {   30, 0x4000 }, {   31, 0x0000 },
  {   31, 0x0020 },
};
static const Summary16 hkscs2008_uni2indx_page84[34] = {
  /* 0x8400 */
  {   32, 0x0000 }, {   32, 0x0000 }, {   32, 0x0000 }, {   32, 0x0000 },
  {   32, 0x0000 }, {   32, 0x0000 }, {   32, 0x0000 }, {   32, 0x0000 },
  {   32, 0x0010 }, {   33, 0x0000 }, {   33, 0x0000 }, {   33, 0x0000 },
  {   33, 0x0000 }, {   33, 0x0000 }, {   33, 0x0000 }, {   33, 0x0000 },
  /* 0x8500 */
  {   33, 0x0010 }, {   34, 0x0000 }, {   34, 0x0000 }, {   34, 0x0000 },
  {   34, 0x0000 }, {   34, 0x0000 }, {   34, 0x0000 }, {   34, 0x0000 },
  {   34, 0x0000 }, {   34, 0x0000 }, {   34, 0x0000 }, {   34, 0x0000 },
  {   34, 0x0000 }, {   34, 0x0000 }, {   34, 0x0000 }, {   34, 0x0000 },
  /* 0x8600 */
  {   34, 0x0000 }, {   34, 0x0008 },
};
static const Summary16 hkscs2008_uni2indx_page88[10] = {
  /* 0x8800 */
  {   35, 0x0000 }, {   35, 0x0000 }, {   35, 0x0000 }, {   35, 0x0000 },
  {   35, 0x0000 }, {   35, 0x0000 }, {   35, 0x0000 }, {   35, 0x0000 },
  {   35, 0x0000 }, {   35, 0x2000 },
};
static const Summary16 hkscs2008_uni2indx_page8b[9] = {
  /* 0x8b00 */
  {   36, 0x0000 }, {   36, 0x0000 }, {   36, 0x0000 }, {   36, 0x0000 },
  {   36, 0x0000 }, {   36, 0x0000 }, {   36, 0x0000 }, {   36, 0x0000 },
  {   36, 0x8000 },
};
static const Summary16 hkscs2008_uni2indx_page90[5] = {
  /* 0x9000 */
  {   37, 0x0000 }, {   37, 0x0000 }, {   37, 0x0000 }, {   37, 0x0000 },
  {   37, 0x0040 },
};
static const Summary16 hkscs2008_uni2indx_page92[2] = {
  /* 0x9200 */
  {   38, 0x0000 }, {   38, 0x0100 },
};
static const Summary16 hkscs2008_uni2indx_page94[3] = {
  /* 0x9400 */
  {   39, 0x0000 }, {   39, 0x0000 }, {   39, 0x8000 },
};
static const Summary16 hkscs2008_uni2indx_page97[5] = {
  /* 0x9700 */
  {   40, 0x0000 }, {   40, 0x0000 }, {   40, 0x0000 }, {   40, 0x0000 },
  {   40, 0x0400 },
};
static const Summary16 hkscs2008_uni2indx_page9f[13] = {
  /* 0x9f00 */
  {   41, 0x0000 }, {   41, 0x0000 }, {   41, 0x0000 }, {   41, 0x0000 },
  {   41, 0x0000 }, {   41, 0x0000 }, {   41, 0x0000 }, {   41, 0x0000 },
  {   41, 0x0000 }, {   41, 0x00c0 }, {   43, 0x0000 }, {   43, 0x0000 },
  {   43, 0x0f80 },
};
static const Summary16 hkscs2008_uni2indx_page20a[9] = {
  /* 0x20a00 */
  {   48, 0x0000 }, {   48, 0x0000 }, {   48, 0x0000 }, {   48, 0x0000 },
  {   48, 0x0000 }, {   48, 0x0000 }, {   48, 0x0000 }, {   48, 0x0000 },
  {   48, 0x0400 },
};
static const Summary16 hkscs2008_uni2indx_page21d[6] = {
  /* 0x21d00 */
  {   49, 0x0000 }, {   49, 0x0000 }, {   49, 0x0000 }, {   49, 0x0000 },
  {   49, 0x0000 }, {   49, 0x0008 },
};
static const Summary16 hkscs2008_uni2indx_page224[13] = {
  /* 0x22400 */
  {   50, 0x0000 }, {   50, 0x0000 }, {   50, 0x0000 }, {   50, 0x0000 },
  {   50, 0x0000 }, {   50, 0x0000 }, {   50, 0x0000 }, {   50, 0x0000 },
  {   50, 0x0000 }, {   50, 0x0000 }, {   50, 0x0000 }, {   50, 0x1000 },
  {   51, 0x1202 },
};
static const Summary16 hkscs2008_uni2indx_page231[22] = {
  /* 0x23100 */
  {   54, 0x0000 }, {   54, 0x0000 }, {   54, 0x0000 }, {   54, 0x0000 },
  {   54, 0x0000 }, {   54, 0x0000 }, {   54, 0x0000 }, {   54, 0x0000 },
  {   54, 0x0000 }, {   54, 0x0000 }, {   54, 0x0000 }, {   54, 0x0000 },
  {   54, 0x0000 }, {   54, 0x0000 }, {   54, 0x0400 }, {   55, 0x0000 },
  /* 0x23200 */
  {   55, 0x0000 }, {   55, 0x0000 }, {   55, 0x0000 }, {   55, 0x0000 },
  {   55, 0x0000 }, {   55, 0x4000 },
};
static const Summary16 hkscs2008_uni2indx_page235[26] = {
  /* 0x23500 */
  {   56, 0x0000 }, {   56, 0x0000 }, {   56, 0x0000 }, {   56, 0x0000 },
  {   56, 0x0000 }, {   56, 0x0000 }, {   56, 0x0000 }, {   56, 0x0000 },
  {   56, 0x0000 }, {   56, 0x0000 }, {   56, 0x0000 }, {   56, 0x0800 },
  {   57, 0x0000 }, {   57, 0x0000 }, {   57, 0x0000 }, {   57, 0x0000 },
  /* 0x23600 */
  {   57, 0x0000 }, {   57, 0x0000 }, {   57, 0x0000 }, {   57, 0x0000 },
  {   57, 0x0000 }, {   57, 0x0000 }, {   57, 0x0000 }, {   57, 0x0000 },
  {   57, 0x4000 }, {   58, 0x4000 },
};
static const Summary16 hkscs2008_uni2indx_page241[7] = {
  /* 0x24100 */
  {   59, 0x0000 }, {   59, 0x0000 }, {   59, 0x0000 }, {   59, 0x0000 },
  {   59, 0x0000 }, {   59, 0x0000 }, {   59, 0x0002 },
};
static const Summary16 hkscs2008_uni2indx_page258[14] = {
  /* 0x25800 */
  {   60, 0x0000 }, {   60, 0x0000 }, {   60, 0x0000 }, {   60, 0x0000 },
  {   60, 0x0000 }, {   60, 0x0000 }, {   60, 0x0000 }, {   60, 0x0000 },
  {   60, 0x0000 }, {   60, 0x0000 }, {   60, 0x0000 }, {   60, 0x0000 },
  {   60, 0x0000 }, {   60, 0x4000 },
};
static const Summary16 hkscs2008_uni2indx_page25d[12] = {
  /* 0x25d00 */
  {   61, 0x0000 }, {   61, 0x0000 }, {   61, 0x0000 }, {   61, 0x0000 },
  {   61, 0x0000 }, {   61, 0x0000 }, {   61, 0x0000 }, {   61, 0x0000 },
  {   61, 0x0000 }, {   61, 0x0200 }, {   62, 0x0000 }, {   62, 0x0200 },
};
static const Summary16 hkscs2008_uni2indx_page260[3] = {
  /* 0x26000 */
  {   63, 0x0000 }, {   63, 0x0000 }, {   63, 0x0002 },
};
static const Summary16 hkscs2008_uni2indx_page26e[9] = {
  /* 0x26e00 */
  {   64, 0x0000 }, {   64, 0x0000 }, {   64, 0x0000 }, {   64, 0x0000 },
  {   64, 0x0000 }, {   64, 0x0000 }, {   64, 0x0000 }, {   64, 0x0000 },
  {   64, 0x0100 },
};
static const Summary16 hkscs2008_uni2indx_page27b[7] = {
  /* 0x27b00 */
  {   65, 0x0000 }, {   65, 0x0000 }, {   65, 0x0000 }, {   65, 0x0000 },
  {   65, 0x0000 }, {   65, 0x0000 }, {   65, 0x0020 },
};
static const Summary16 hkscs2008_uni2indx_page289[1] = {
  /* 0x28900 */
  {   66, 0x2000 },
};
static const Summary16 hkscs2008_uni2indx_page2ad[16] = {
  /* 0x2ad00 */
  {   67, 0x0000 }, {   67, 0x0000 }, {   67, 0x0000 }, {   67, 0x0000 },
  {   67, 0x0000 }, {   67, 0x0000 }, {   67, 0x0000 }, {   67, 0x0000 },
  {   67, 0x0000 }, {   67, 0x0000 }, {   67, 0x0000 }, {   67, 0x0000 },
  {   67, 0x0000 }, {   67, 0x0000 }, {   67, 0x0000 }, {   67, 0x8000 },
};

static int
hkscs2008_wctomb (conv_t conv, unsigned char *r, ucs4_t wc, size_t n)
{
  if (n >= 2) {
    const Summary16 *summary = NULL;
    if (wc >= 0x3400 && wc < 0x34f0)
      summary = &hkscs2008_uni2indx_page34[(wc>>4)-0x340];
    else if (wc >= 0x3800 && wc < 0x3880)
      summary = &hkscs2008_uni2indx_page38[(wc>>4)-0x380];
    else if (wc >= 0x3a00 && wc < 0x3b00)
      summary = &hkscs2008_uni2indx_page3a[(wc>>4)-0x3a0];
    else if (wc >= 0x3e00 && wc < 0x3ef0)
      summary = &hkscs2008_uni2indx_page3e[(wc>>4)-0x3e0];
    else if (wc >= 0x4000 && wc < 0x4190)
      summary = &hkscs2008_uni2indx_page40[(wc>>4)-0x400];
    else if (wc >= 0x4300 && wc < 0x44f0)
      summary = &hkscs2008_uni2indx_page43[(wc>>4)-0x430];
    else if (wc >= 0x4600 && wc < 0x46b0)
      summary = &hkscs2008_uni2indx_page46[(wc>>4)-0x460];
    else if (wc >= 0x4900 && wc < 0x4940)
      summary = &hkscs2008_uni2indx_page49[(wc>>4)-0x490];
    else if (wc >= 0x5200 && wc < 0x5250)
      summary = &hkscs2008_uni2indx_page52[(wc>>4)-0x520];
    else if (wc >= 0x5400 && wc < 0x5450)
      summary = &hkscs2008_uni2indx_page54[(wc>>4)-0x540];
    else if (wc >= 0x5700 && wc < 0x58a0)
      summary = &hkscs2008_uni2indx_page57[(wc>>4)-0x570];
    else if (wc >= 0x6200 && wc < 0x62d0)
      summary = &hkscs2008_uni2indx_page62[(wc>>4)-0x620];
    else if (wc >= 0x6600 && wc < 0x6790)
      summary = &hkscs2008_uni2indx_page66[(wc>>4)-0x660];
    else if (wc >= 0x6a00 && wc < 0x6a30)
      summary = &hkscs2008_uni2indx_page6a[(wc>>4)-0x6a0];
    else if (wc >= 0x7000 && wc < 0x7070)
      summary = &hkscs2008_uni2indx_page70[(wc>>4)-0x700];
    else if (wc >= 0x7300 && wc < 0x74d0)
      summary = &hkscs2008_uni2indx_page73[(wc>>4)-0x730];
    else if (wc >= 0x7900 && wc < 0x7bd0)
      summary = &hkscs2008_uni2indx_page79[(wc>>4)-0x790];
    else if (wc >= 0x8400 && wc < 0x8620)
      summary = &hkscs2008_uni2indx_page84[(wc>>4)-0x840];
    else if (wc >= 0x8800 && wc < 0x88a0)
      summary = &hkscs2008_uni2indx_page88[(wc>>4)-0x880];
    else if (wc >= 0x8b00 && wc < 0x8b90)
      summary = &hkscs2008_uni2indx_page8b[(wc>>4)-0x8b0];
    else if (wc >= 0x9000 && wc < 0x9050)
      summary = &hkscs2008_uni2indx_page90[(wc>>4)-0x900];
    else if (wc >= 0x9200 && wc < 0x9220)
      summary = &hkscs2008_uni2indx_page92[(wc>>4)-0x920];
    else if (wc >= 0x9400 && wc < 0x9430)
      summary = &hkscs2008_uni2indx_page94[(wc>>4)-0x940];
    else if (wc >= 0x9700 && wc < 0x9750)
      summary = &hkscs2008_uni2indx_page97[(wc>>4)-0x970];
    else if (wc >= 0x9f00 && wc < 0x9fd0)
      summary = &hkscs2008_uni2indx_page9f[(wc>>4)-0x9f0];
    else if (wc >= 0x20a00 && wc < 0x20a90)
      summary = &hkscs2008_uni2indx_page20a[(wc>>4)-0x20a0];
    else if (wc >= 0x21d00 && wc < 0x21d60)
      summary = &hkscs2008_uni2indx_page21d[(wc>>4)-0x21d0];
    else if (wc >= 0x22400 && wc < 0x224d0)
      summary = &hkscs2008_uni2indx_page224[(wc>>4)-0x2240];
    else if (wc >= 0x23100 && wc < 0x23260)
      summary = &hkscs2008_uni2indx_page231[(wc>>4)-0x2310];
    else if (wc >= 0x23500 && wc < 0x236a0)
      summary = &hkscs2008_uni2indx_page235[(wc>>4)-0x2350];
    else if (wc >= 0x24100 && wc < 0x24170)
      summary = &hkscs2008_uni2indx_page241[(wc>>4)-0x2410];
    else if (wc >= 0x25800 && wc < 0x258e0)
      summary = &hkscs2008_uni2indx_page258[(wc>>4)-0x2580];
    else if (wc >= 0x25d00 && wc < 0x25dc0)
      summary = &hkscs2008_uni2indx_page25d[(wc>>4)-0x25d0];
    else if (wc >= 0x26000 && wc < 0x26030)
      summary = &hkscs2008_uni2indx_page260[(wc>>4)-0x2600];
    else if (wc >= 0x26e00 && wc < 0x26e90)
      summary = &hkscs2008_uni2indx_page26e[(wc>>4)-0x26e0];
    else if (wc >= 0x27b00 && wc < 0x27b70)
      summary = &hkscs2008_uni2indx_page27b[(wc>>4)-0x27b0];
    else if (wc >= 0x28900 && wc < 0x28910)
      summary = &hkscs2008_uni2indx_page289[(wc>>4)-0x2890];
    else if (wc >= 0x2ad00 && wc < 0x2ae00)
      summary = &hkscs2008_uni2indx_page2ad[(wc>>4)-0x2ad0];
    if (summary) {
      unsigned short used = summary->used;
      unsigned int i = wc & 0x0f;
      if (used & ((unsigned short) 1 << i)) {
        unsigned short c;
        /* Keep in 'used' only the bits 0..i-1. */
        used &= ((unsigned short) 1 << i) - 1;
        /* Add 'summary->indx' and the number of bits set in 'used'. */
        used = (used & 0x5555) + ((used & 0xaaaa) >> 1);
        used = (used & 0x3333) + ((used & 0xcccc) >> 2);
        used = (used & 0x0f0f) + ((used & 0xf0f0) >> 4);
        used = (used & 0x00ff) + (used >> 8);
        c = hkscs2008_2charset[summary->indx + used];
        r[0] = (c >> 8); r[1] = (c & 0xff);
        return 2;
      }
    }
    return RET_ILUNI;
  }
  return RET_TOOSMALL;
}
