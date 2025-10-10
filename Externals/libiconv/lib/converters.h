/*
 * Copyright (C) 1999-2024 Free Software Foundation, Inc.
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

/* This file defines all the converters. */


/* Our own notion of wide character, as UCS-4, according to ISO-10646-1. */
typedef unsigned int ucs4_t;

/* State used by a conversion. 0 denotes the initial state. */
typedef unsigned int state_t;

/* iconv_t is an opaque type. This is the real iconv_t type. */
typedef struct conv_struct * conv_t;

/*
 * Data type for conversion multibyte -> unicode
 */
struct mbtowc_funcs {
  int (*xxx_mbtowc) (conv_t conv, ucs4_t *pwc, unsigned char const *s, size_t n);
  /*
   * int xxx_mbtowc (conv_t conv, ucs4_t *pwc, unsigned char const *s, size_t n)
   * converts the byte sequence starting at s to a wide character. Up to n bytes
   * are available at s. n is >= 1.
   * Result is number of bytes consumed (if a wide character was read),
   * or -1 if invalid, or -2 if n too small,
   * or RET_SHIFT_ILSEQ(number of bytes consumed) if invalid input after a shift
   * sequence was read,
   * or RET_TOOFEW(number of bytes consumed) if only a shift sequence was read.
   */
  int (*xxx_flushwc) (conv_t conv, ucs4_t *pwc);
  /*
   * int xxx_flushwc (conv_t conv, ucs4_t *pwc)
   * returns to the initial state and stores the pending wide character, if any.
   * Result is 1 (if a wide character was read) or 0 if none was pending.
   */
};

/* Return code if invalid input after a shift sequence of n bytes was read.
   (xxx_mbtowc) */
#define RET_SHIFT_ILSEQ(n)  (-1-2*(n))
/* Return code if invalid. (xxx_mbtowc) */
#define RET_ILSEQ           RET_SHIFT_ILSEQ(0)
/* Return code if only a shift sequence of n bytes was read. (xxx_mbtowc) */
#define RET_TOOFEW(n)       (-2-2*(n))
/* Retrieve the n from the encoded RET_... value. */
#define DECODE_SHIFT_ILSEQ(r)  ((unsigned int)(RET_SHIFT_ILSEQ(0) - (r)) / 2)
#define DECODE_TOOFEW(r)       ((unsigned int)(RET_TOOFEW(0) - (r)) / 2)
/* Maximum value of n that may be used as argument to RET_SHIFT_ILSEQ or RET_TOOFEW. */
#define RET_COUNT_MAX       ((INT_MAX / 2) - 1)

/*
 * Data type for conversion unicode -> multibyte
 */
struct wctomb_funcs {
  int (*xxx_wctomb) (conv_t conv, unsigned char *r, ucs4_t wc, size_t n);
  /*
   * int xxx_wctomb (conv_t conv, unsigned char *r, ucs4_t wc, size_t n)
   * converts the wide character wc to the character set xxx, and stores the
   * result beginning at r. Up to n bytes may be written at r. n is >= 1.
   * Result is number of bytes written, or -1 if invalid, or -2 if n too small.
   */
  int (*xxx_reset) (conv_t conv, unsigned char *r, size_t n);
  /*
   * int xxx_reset (conv_t conv, unsigned char *r, size_t n)
   * stores a shift sequences returning to the initial state beginning at r.
   * Up to n bytes may be written at r. n is >= 0.
   * Result is number of bytes written, or -2 if n too small.
   */
};

/* Return code if invalid. (xxx_wctomb) */
#define RET_ILUNI      -1
/* Return code if output buffer is too small. (xxx_wctomb, xxx_reset) */
#define RET_TOOSMALL   -2

/*
 * Contents of a conversion descriptor.
 */
struct conv_struct {
  struct loop_funcs lfuncs;
  /* Input (conversion multibyte -> unicode) */
  int iindex;
  struct mbtowc_funcs ifuncs;
  unsigned int isurface;
  state_t ibyteorder;
  state_t istate;
  /* Output (conversion unicode -> multibyte) */
  int oindex;
  struct wctomb_funcs ofuncs;
  int oflags;
  unsigned int osurface;
  state_t ostate;
  /* Operation flags */
  int transliterate;
  unsigned int discard_ilseq;
  struct iconv_fallbacks fallbacks;
  struct iconv_hooks hooks;
};

/* The bits in discard_ilseq.  */
/* Bit mask for discard invalid multibyte sequences in the input. */
#define DISCARD_INVALID        2U
/* Bit mask for discard characters that cannot be represented in the target
   character set. */
#define DISCARD_UNCONVERTIBLE  1U

/*
 * Include all the converters.
 */

#include "ascii.h"

/* General multi-byte encodings */
#include "utf8.h"
#include "ucs2.h"
#include "ucs2be.h"
#include "ucs2le.h"
#include "ucs4.h"
#include "ucs4be.h"
#include "ucs4le.h"
#include "utf16.h"
#include "utf16be.h"
#include "utf16le.h"
#include "utf32.h"
#include "utf32be.h"
#include "utf32le.h"
#include "utf7.h"
#include "ucs2internal.h"
#include "ucs2swapped.h"
#include "ucs4internal.h"
#include "ucs4swapped.h"
#include "c99.h"
#include "java.h"

/* 8-bit encodings */
#include "iso8859_1.h"
#include "iso8859_2.h"
#include "iso8859_3.h"
#include "iso8859_4.h"
#include "iso8859_5.h"
#include "iso8859_6.h"
#include "iso8859_7.h"
#include "iso8859_8.h"
#include "iso8859_9.h"
#include "iso8859_10.h"
#include "iso8859_11.h"
#include "iso8859_13.h"
#include "iso8859_14.h"
#include "iso8859_15.h"
#include "iso8859_16.h"
#include "koi8_r.h"
#include "koi8_u.h"
#include "koi8_ru.h"
#include "cp1250.h"
#include "cp1251.h"
#include "cp1252.h"
#include "cp1253.h"
#include "cp1254.h"
#include "cp1255.h"
#include "cp1256.h"
#include "cp1257.h"
#include "cp1258.h"
#include "cp850.h"
#include "cp862.h"
#include "cp866.h"
#include "cp1131.h"
#include "mac_roman.h"
#include "mac_centraleurope.h"
#include "mac_iceland.h"
#include "mac_croatian.h"
#include "mac_romania.h"
#include "mac_cyrillic.h"
#include "mac_ukraine.h"
#include "mac_greek.h"
#include "mac_turkish.h"
#include "mac_hebrew.h"
#include "mac_arabic.h"
#include "mac_thai.h"
#include "hp_roman8.h"
#include "nextstep.h"
#include "armscii_8.h"
#include "georgian_academy.h"
#include "georgian_ps.h"
#include "koi8_t.h"
#include "pt154.h"
#include "rk1048.h"
#include "mulelao.h"
#include "cp1133.h"
#include "tis620.h"
#include "cp874.h"
#include "viscii.h"
#include "tcvn.h"

/* CJK character sets [CCS = coded character set] [CJKV.INF chapter 3] */

typedef struct {
  unsigned short indx; /* index into big table */
  unsigned short used; /* bitmask of used entries */
} Summary16;

#include "iso646_jp.h"
#include "jisx0201.h"
#include "jisx0208.h"
#include "jisx0212.h"

#include "iso646_cn.h"
#include "gb2312.h"
#include "isoir165.h"
/*#include "gb12345.h"*/
#include "gbk.h"
#include "cns11643.h"
#include "big5.h"

#include "ksc5601.h"
#include "johab_hangul.h"

/* CJK encodings [CES = character encoding scheme] [CJKV.INF chapter 4] */

#include "euc_jp.h"
#include "sjis.h"
#include "cp932.h"
#include "iso2022_jp.h"
#include "iso2022_jp1.h"
#include "iso2022_jp2.h"
#include "iso2022_jpms.h"

#include "euc_cn.h"
#include "ces_gbk.h"
#include "cp936.h"
#include "gb18030_2005.h"
#include "gb18030_2022.h"
#include "iso2022_cn.h"
#include "iso2022_cnext.h"
#include "hz.h"
#include "euc_tw.h"
#include "ces_big5.h"
#include "cp950.h"
#include "big5hkscs1999.h"
#include "big5hkscs2001.h"
#include "big5hkscs2004.h"
#include "big5hkscs2008.h"

#include "euc_kr.h"
#include "cp949.h"
#include "johab.h"
#include "iso2022_kr.h"

/* Encodings used by system dependent locales. */

#ifdef USE_AIX
#include "cp856.h"
#include "cp922.h"
#include "cp943.h"
#include "cp1046.h"
#include "cp1124.h"
#include "cp1129.h"
#include "cp1161.h"
#include "cp1162.h"
#include "cp1163.h"
#endif

#ifdef USE_OSF1
#include "dec_kanji.h"
#include "dec_hanyu.h"
#endif

#ifdef USE_DOS
#include "cp437.h"
#include "cp737.h"
#include "cp775.h"
#include "cp852.h"
#include "cp853.h"
#include "cp855.h"
#include "cp857.h"
#include "cp858.h"
#include "cp860.h"
#include "cp861.h"
#include "cp863.h"
#include "cp864.h"
#include "cp865.h"
#include "cp869.h"
#include "cp1125.h"
#endif

#ifdef USE_ZOS

#define DEDUPLICATE_TABLES 1

/* Swaps the values 0x15 and 0x25.
   Both gcc and clang compile this expression to something that involves as few
   conditional branching instructions as possible. */
#define swap_x15_x25_a(x) ((x) == 0x15 ? 0x25 : (x) == 0x25 ? 0x15 : (x))
#define swap_x15_x25_b(x) ((x) ^ ((x) == 0x15 || (x) == 0x25 ? 0x30 : 0))
#define swap_x15_x25_c(x) ((x) ^ ((((x) - 0x15) & ~0x10) == 0 ? 0x30 : 0))
/* Number of conditional branches (with "gcc -O2", as of 2023):
                   a    b    c
                 ---------------
   aarch64         1    0    0
   alpha           0    0    0
   arm             1    0    0
   hppa            1    1    1
   i686            1    0    0
   m68k            2    1    1
   mips            2    1    0
   mips64          2    1    0
   powerpc         2    1    1
   powerpc64       2    1    1
   powerpc64le     2    1    1
   riscv64         2    1    1
   s390x           1    1    1
   sh4             2    1    1
   x86_64          1    0    0
*/
#define swap_x15_x25 swap_x15_x25_c

#include "ebcdic037.h"
#include "ebcdic273.h"
#include "ebcdic277.h"
#include "ebcdic278.h"
#include "ebcdic280.h"
#include "ebcdic282.h"
#include "ebcdic284.h"
#include "ebcdic285.h"
#include "ebcdic297.h"
#include "ebcdic423.h"
#include "ebcdic424.h"
#include "ebcdic425.h"
#include "ebcdic500.h"
#include "ebcdic838.h"
#include "ebcdic870.h"
#include "ebcdic871.h"
#include "ebcdic875.h"
#include "ebcdic880.h"
#include "ebcdic905.h"
#include "ebcdic924.h"
#include "ebcdic1025.h"
#include "ebcdic1026.h"
#include "ebcdic1047.h"
#include "ebcdic1097.h"
#include "ebcdic1112.h"
#include "ebcdic1122.h"
#include "ebcdic1123.h"
#include "ebcdic1130.h"
#include "ebcdic1132.h"
#include "ebcdic1137.h"
#include "ebcdic1140.h"
#include "ebcdic1141.h"
#include "ebcdic1142.h"
#include "ebcdic1143.h"
#include "ebcdic1144.h"
#include "ebcdic1145.h"
#include "ebcdic1146.h"
#include "ebcdic1147.h"
#include "ebcdic1148.h"
#include "ebcdic1149.h"
#include "ebcdic1153.h"
#include "ebcdic1154.h"
#include "ebcdic1155.h"
#include "ebcdic1156.h"
#include "ebcdic1157.h"
#include "ebcdic1158.h"
#include "ebcdic1160.h"
#include "ebcdic1164.h"
#include "ebcdic1165.h"
#include "ebcdic1166.h"
#include "ebcdic4971.h"
#include "ebcdic12712.h"
#include "ebcdic16804.h"
#endif

#ifdef USE_EXTRA
#include "euc_jisx0213.h"
#include "shift_jisx0213.h"
#include "iso2022_jp3.h"
#include "big5_2003.h"
#include "tds565.h"
#include "atarist.h"
#include "riscos1.h"
#endif

