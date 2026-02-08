/*
 * Copyright (C) 1999-2001, 2008, 2011-2012, 2016, 2018, 2024 Free Software Foundation, Inc.
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
 * ISO-2022-JP-MS
 * alias CP50221
 *
 * This is an extension of ISO-2022-JP-1 with larger character sets.
 * It uses ESC $ B and ESC $ ( D to denote *extensions* of JIS X 0208 and
 * JIS X 0212, respectively.  This violates the principles of ISO 2022,
 * where
 *   1. character sets to be used by ISO 2022 have to be registered at the
 *      ISO IR registry <https://www.itscj.ipsj.or.jp/ISO-IR/>,
 *   2. different character sets are designated by different escape
 *      sequences.
 * It's a typical instance of the "embrace and extend" strategy by Microsoft
 * <https://en.wikipedia.org/wiki/Embrace,_extend_and_extinguish>.
 */

/*
 * Windows has three encodings CP50220, CP50221, CP50222.
 * The common parts are:
 *   - US-ASCII (0x00..0x7F)
 *   - JIS X 0208 extended by
 *       - one row (0x2D),
 *       - a private use area (rows 0x75..0x7E = U+E000..U+E3AB),
 *     enabled with ESC $ B, disabled with ESC ( B.
 *   - JIS X 0212 extended by
 *       - two rows (0x73..0x74),
 *       - a private use area (rows 0x75..0x7E = U+E3AC..U+E757),
 *     enabled with ESC $ ( D, disabled with ESC ( B.
 * They differ in the handling of JIS X 0201 characters (halfwidth Katakana)
 * in the conversion direction Unicode -> CP5022x:
 *   * CP50220 maps the halfwidth Katakana to fullwidth Katakana characters.
 *   * CP50221 contains the JIS X 0201 halfwidth Katakana characters,
 *     enabled with ESC ( I, disabled with ESC ( B.
 *   * CP50222 contains the JIS X 0201 halfwidth Katakana characters,
 *     enabled with ESC ( J 0x0E, disabled with ESC ( B.
 * In the conversion direction CP5022x -> Unicode, all three operate the same:
 *   - ESC ( I is supported and understood.
 *   - ESC ( J 0x0E is not accepted.  (Tested on Windows XP SP3.)
 * Conclusion:
 *   - CP50222 should not be used, because the multibyte sequence that it
 *     produces cannot be parsed by either of the three encodings.
 *   - CP50221 is preferable to CP50220, because it can faithfully represent
 *     the halfwidth Katakana characters.
 * We therefore implement CP50221.  As an extension, in the mbtowc conversion
 * direction, we support also ESC ( J 0x0E, just in case.
 */

#include "cp50221_0208_ext.h"
#include "cp50221_0212_ext.h"

#define ESC 0x1b
#define SO  0x0e
#define SI  0x0f

/*
 * The state can be one of the following values.
 */
#define STATE_ASCII             0  /* Esc ( B */
#define STATE_JISX0201ROMAN     1  /* Esc ( J */ /* only in mbtowc direction */
#define STATE_JISX0201KATAKANA  2  /* Esc ( I */
#define STATE_JISX0208MS        3  /* Esc $ @ or Esc $ B */
#define STATE_JISX0212MS        4  /* Esc $ ( D */

static int
iso2022_jpms_mbtowc (conv_t conv, ucs4_t *pwc, const unsigned char *s, size_t n)
{
  state_t state = conv->istate;
  int count = 0;
  unsigned char c;
  for (;;) {
    c = *s;
    if (c == ESC) {
      if (n < count+3)
        goto none;
      if (s[1] == '(') {
        if (s[2] == 'B') {
          state = STATE_ASCII;
          s += 3; count += 3;
          if (n < count+1)
            goto none;
          continue;
        }
        if (s[2] == 'I') {
          state = STATE_JISX0201KATAKANA;
          s += 3; count += 3;
          if (n < count+1)
            goto none;
          continue;
        }
        if (s[2] == 'J') {
          state = STATE_JISX0201ROMAN;
          s += 3; count += 3;
          if (n < count+1)
            goto none;
          continue;
        }
        goto ilseq;
      }
      if (s[1] == '$') {
        if (s[2] == '@' || s[2] == 'B') {
          /* We don't distinguish JIS X 0208-1978 and JIS X 0208-1983. */
          state = STATE_JISX0208MS;
          s += 3; count += 3;
          if (n < count+1)
            goto none;
          continue;
        }
        if (s[2] == '(') {
          if (n < count+4)
            goto none;
          if (s[3] == 'D') {
            state = STATE_JISX0212MS;
            s += 4; count += 4;
            if (n < count+1)
              goto none;
            continue;
          }
        }
        goto ilseq;
      }
      goto ilseq;
    }
    if (c == SO) {
      if (state == STATE_JISX0201ROMAN)
        state = STATE_JISX0201KATAKANA;
      s += 1; count += 1;
      if (n < count+1)
        goto none;
      continue;
    }
    if (c == SI) {
      if (state == STATE_JISX0201KATAKANA)
        state = STATE_JISX0201ROMAN;
      s += 1; count += 1;
      if (n < count+1)
        goto none;
      continue;
    }
    break;
  }
  switch (state) {
    case STATE_ASCII:
      if (c < 0x80) {
        int ret = ascii_mbtowc(conv,pwc,s,1);
        if (ret == RET_ILSEQ)
          goto ilseq;
        if (ret != 1) abort();
        conv->istate = state;
        return count+1;
      } else
        goto ilseq;
    case STATE_JISX0201ROMAN:
      if (c < 0x80) {
        int ret = jisx0201_mbtowc(conv,pwc,s,1);
        if (ret == RET_ILSEQ)
          goto ilseq;
        if (ret != 1) abort();
        conv->istate = state;
        return count+1;
      } else
        goto ilseq;
    case STATE_JISX0201KATAKANA:
      if (c < 0x80) {
        unsigned char buf = c+0x80;
        int ret = jisx0201_mbtowc(conv,pwc,&buf,1);
        if (ret == RET_ILSEQ)
          goto ilseq;
        if (ret != 1) abort();
        conv->istate = state;
        return count+1;
      } else
        goto ilseq;
    case STATE_JISX0208MS:
      if (n < count+2)
        goto none;
      if (s[0] < 0x80 && s[1] < 0x80) {
        int ret;
        if (s[0] < 0x75) {
          if (s[0] == 0x2d) {
            /* Extension of JIS X 0208.  */
            if (s[1] >= 0x21 && s[1] <= 0x79) {
              unsigned char i = (s[1] - 0x21) + 1;
              ret = cp50221_0208_ext_mbtowc(conv,pwc,&i,1);
              if (ret == 1)
                ret = 2;
            } else
              ret = RET_ILSEQ;
          } else {
            /* JIS X 0208.  */
            ret = jisx0208_mbtowc(conv,pwc,s,2);
          }
        } else {
          /* Extension of JIS X 0208.
             0x{75..7E}{21..8E} maps to U+E000..U+E3AB.
             But some rows maps to characters present in CP932.  */
          if (s[0] <= 0x7e && (s[1] >= 0x21 && s[1] <= 0x7e)) {
            unsigned short wc = 0xfffd;
            if (s[0] >= 0x79 && s[0] <= 0x7c)
              wc = cp932ext_2uni_pageed[(s[0] - 0x79) * 94 + (s[1] - 0x21)];
            if (wc == 0xfffd)
              wc = (s[0] - 0x75) * 94 + (s[1] - 0x21) + 0xe000;
            *pwc = wc;
            ret = 2;
          } else
            ret = RET_ILSEQ;
        }
        if (ret == RET_ILSEQ)
          goto ilseq;
        if (ret != 2) abort();
        conv->istate = state;
        return count+2;
      } else
        goto ilseq;
    case STATE_JISX0212MS:
      if (n < count+2)
        goto none;
      if (s[0] < 0x80 && s[1] < 0x80) {
        int ret;
        if (s[0] < 0x73) {
          /* JIS X 0212.  */
          ret = jisx0212_mbtowc(conv,pwc,s,2);
        } else {
          if (s[0] < 0x75) {
            /* Extension of JIS X 0212.  */
            if (s[1] >= 0x21 && s[1] <= 0x7e) {
              unsigned char i = (s[0] - 0x73) * 94 + (s[1] - 0x21) + 1;
              ret = cp50221_0212_ext_mbtowc(conv,pwc,&i,1);
              if (ret == 1)
                ret = 2;
            } else
              ret = RET_ILSEQ;
          } else {
            /* Extension of JIS X 0208.
               0x{75..7E}{21..8E} maps to U+E3AC..U+E757.  */
            if (s[0] <= 0x7e && (s[1] >= 0x21 && s[1] <= 0x7e)) {
              *pwc = (s[0] - 0x75) * 94 + (s[1] - 0x21) + 0xe3ac;
              ret = 2;
            } else
              ret = RET_ILSEQ;
          }
        }
        if (ret == RET_ILSEQ)
          goto ilseq;
        if (ret != 2) abort();
        conv->istate = state;
        return count+2;
      } else
        goto ilseq;
    default: abort();
  }

none:
  conv->istate = state;
  return RET_TOOFEW(count);

ilseq:
  conv->istate = state;
  return RET_SHIFT_ILSEQ(count);
}

static int
iso2022_jpms_wctomb (conv_t conv, unsigned char *r, ucs4_t wc, size_t n)
{
  state_t state = conv->ostate;
  unsigned char buf[2];
  int ret;

  /* Try ASCII. */
  ret = ascii_wctomb(conv,buf,wc,1);
  if (ret != RET_ILUNI) {
    if (ret != 1) abort();
    if (buf[0] < 0x80) {
      int count = (state == STATE_ASCII ? 1 : 4);
      if (n < count)
        return RET_TOOSMALL;
      if (state != STATE_ASCII) {
        r[0] = ESC;
        r[1] = '(';
        r[2] = 'B';
        r += 3;
        state = STATE_ASCII;
      }
      r[0] = buf[0];
      conv->ostate = state;
      return count;
    }
  }

  /* Try JIS X 0201-1976 Katakana. */
  ret = jisx0201_wctomb(conv,buf,wc,1);
  if (ret != RET_ILUNI) {
    if (ret != 1) abort();
    if (buf[0] >= 0x80) {
      int count = (state == STATE_JISX0201KATAKANA ? 1 : 4);
      if (n < count)
        return RET_TOOSMALL;
      if (state != STATE_JISX0201KATAKANA) {
        r[0] = ESC;
        r[1] = '(';
        r[2] = 'I';
        r += 3;
        state = STATE_JISX0201KATAKANA;
      }
      r[0] = buf[0]-0x80;
      conv->ostate = state;
      return count;
    }
  }

  /* Try JIS X 0208-1990, in place of JIS X 0208-1978 and JIS X 0208-1983,
     and the extensions mentioned above.  */
  if (wc >= 0xe000 && wc < 0xe3ac) {
    unsigned short i = wc - 0xe000;
    buf[0] = (i / 94) + 0x75;
    buf[1] = (i % 94) + 0x21;
    ret = 2;
  } else {
    ret = jisx0208_wctomb(conv,buf,wc,2);
    if (ret == RET_ILUNI) {
      /* Extension of JIS X 0208.  */
      unsigned char i;
      ret = cp50221_0208_ext_wctomb(conv,&i,wc,1);
      if (ret == 1) {
        buf[0] = 0x2d;
        buf[1] = i-1 + 0x21;
        ret = 2;
      } else if (wc == 0x663B) {
        buf[0] = 0x7a;
        buf[1] = 0x36;
        ret = 2;
      } else if (wc == 0xffe2) {
        buf[0] = 0x7c;
        buf[1] = 0x7b;
        ret = 2;
      } else if (wc == 0xffe4) {
        buf[0] = 0x7c;
        buf[1] = 0x7c;
        ret = 2;
      }
    }
  }
  if (ret != RET_ILUNI) {
    if (ret != 2) abort();
    if (buf[0] < 0x80 && buf[1] < 0x80) {
      int count = (state == STATE_JISX0208MS ? 2 : 5);
      if (n < count)
        return RET_TOOSMALL;
      if (state != STATE_JISX0208MS) {
        r[0] = ESC;
        r[1] = '$';
        r[2] = 'B';
        r += 3;
        state = STATE_JISX0208MS;
      }
      r[0] = buf[0];
      r[1] = buf[1];
      conv->ostate = state;
      return count;
    }
  }

  /* Try JIS X 0212-1990 and the extensions mentioned above. */
  if (wc >= 0xe3ac && wc < 0xe758) {
    unsigned short i = wc - 0xe3ac;
    buf[0] = (i / 94) + 0x75;
    buf[1] = (i % 94) + 0x21;
    ret = 2;
  } else {
    ret = jisx0212_wctomb(conv,buf,wc,2);
    if (ret == RET_ILUNI) {
      /* Extension of JIS X 0212.  */
      unsigned char i;
      ret = cp50221_0212_ext_wctomb(conv,&i,wc,1);
      if (ret == 1) {
        i -= 1;
        buf[0] = (i / 94) + 0x73;
        buf[1] = (i % 94) + 0x21;
        ret = 2;
      }
    }
  }
  if (ret != RET_ILUNI) {
    if (ret != 2) abort();
    if (buf[0] < 0x80 && buf[1] < 0x80) {
      int count = (state == STATE_JISX0212MS ? 2 : 6);
      if (n < count)
        return RET_TOOSMALL;
      if (state != STATE_JISX0212MS) {
        r[0] = ESC;
        r[1] = '$';
        r[2] = '(';
        r[3] = 'D';
        r += 4;
        state = STATE_JISX0212MS;
      }
      r[0] = buf[0];
      r[1] = buf[1];
      conv->ostate = state;
      return count;
    }
  }

  return RET_ILUNI;
}

static int
iso2022_jpms_reset (conv_t conv, unsigned char *r, size_t n)
{
  state_t state = conv->ostate;
  if (state != STATE_ASCII) {
    if (n < 3)
      return RET_TOOSMALL;
    r[0] = ESC;
    r[1] = '(';
    r[2] = 'B';
    /* conv->ostate = 0; will be done by the caller */
    return 3;
  } else
    return 0;
}

#undef STATE_JISX0212MS
#undef STATE_JISX0208MS
#undef STATE_JISX0201KATAKANA
#undef STATE_JISX0201ROMAN
#undef STATE_ASCII
