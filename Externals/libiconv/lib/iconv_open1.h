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

/* Part 1 of iconv_open.
   Input: const char* tocode, const char* fromcode.
   Output:
     unsigned int from_index;
     int from_wchar;
     unsigned int from_surface;
     unsigned int to_index;
     int to_wchar;
     unsigned int to_surface;
     int transliterate;
     unsigned int discard_ilseq;
   Jumps to 'invalid' in case of error.
 */
{
  char buf[MAX_WORD_LENGTH+9+9+1];
  const char* cp;
  char* bp;
  const struct alias * ap;
  unsigned int count;

  from_surface = ICONV_SURFACE_NONE;
  to_surface = ICONV_SURFACE_NONE;
  transliterate = 0;
  discard_ilseq = 0;

  /* Before calling aliases_lookup, convert the input string to upper case,
   * and check whether it's entirely ASCII (we call gperf with option "-7"
   * to achieve a smaller table) and non-empty. If it's not entirely ASCII,
   * or if it's too long, it is not a valid encoding name.
   */
  for (to_wchar = 0;;) {
    /* Search tocode in the table. */
    for (cp = tocode, bp = buf, count = MAX_WORD_LENGTH+9+9+1; ; cp++, bp++) {
      unsigned char c = (unsigned char) *cp;
      if (c >= 0x80)
        goto invalid;
      if (c >= 'a' && c <= 'z')
        c -= 'a'-'A';
      *bp = c;
      if (c == '\0')
        break;
      if (--count == 0)
        goto invalid;
    }
    for (;;) {
      char *sp = bp;
      int parsed_translit = 0;
      int parsed_ignore = 0;
      int parsed_non_identical_discard = 0;
      for (;;) {
        if (sp-buf > 9 && memcmp(sp-9,"/TRANSLIT",9)==0) {
          sp = sp - 9;
          parsed_translit = 1;
        } else if (sp-buf > 7 && memcmp(sp-7,"/IGNORE",7)==0) {
          sp = sp - 7;
          parsed_ignore = 1;
        } else if (sp-buf > 22 && memcmp(sp-22,"/NON_IDENTICAL_DISCARD",22)==0) {
          sp = sp - 22;
          parsed_non_identical_discard = 1;
        } else
          break;
        if (sp > buf && sp[-1] == '/')
          sp = sp - 1;
      }
      if (sp > buf && sp[-1] == '/') {
        bp = sp - 1;
      } else if (sp-buf >= 9 && memcmp(sp-9,"/ZOS_UNIX",9)==0) {
        bp = sp - 9;
        to_surface = ICONV_SURFACE_EBCDIC_ZOS_UNIX;
      } else
        bp = sp;
      *bp = '\0';
      if (parsed_translit)
        transliterate = 1;
      if (parsed_ignore)
        discard_ilseq |= DISCARD_INVALID | DISCARD_UNCONVERTIBLE;
      if (parsed_non_identical_discard)
        discard_ilseq |= DISCARD_UNCONVERTIBLE;
      break;
    }
    if (buf[0] == '\0') {
      tocode = locale_charset();
      /* Avoid an endless loop that could occur when using an older version
         of localcharset.c. */
      if (tocode[0] == '\0')
        goto invalid;
      continue;
    }
    ap = aliases_lookup(buf,bp-buf);
    if (ap == NULL) {
      ap = aliases2_lookup(buf);
      if (ap == NULL)
        goto invalid;
    }
    if (ap->encoding_index == ei_local_char) {
      tocode = locale_charset();
      /* Avoid an endless loop that could occur when using an older version
         of localcharset.c. */
      if (tocode[0] == '\0')
        goto invalid;
      continue;
    }
    if (ap->encoding_index == ei_local_wchar_t) {
      /* On systems which define __STDC_ISO_10646__, wchar_t is Unicode.
         This is also the case on native Woe32 systems and Cygwin >= 1.7, where
         we know that it is UTF-16.  */
#if (defined _WIN32 && !defined __CYGWIN__) || (defined __CYGWIN__ && CYGWIN_VERSION_DLL_MAJOR >= 1007)
      if (sizeof(wchar_t) == 4) {
        to_index = ei_ucs4internal;
        break;
      }
      if (sizeof(wchar_t) == 2) {
# if WORDS_LITTLEENDIAN
        to_index = ei_utf16le;
# else
        to_index = ei_utf16be;
# endif
        break;
      }
#elif __STDC_ISO_10646__
      if (sizeof(wchar_t) == 4) {
        to_index = ei_ucs4internal;
        break;
      }
      if (sizeof(wchar_t) == 2) {
        to_index = ei_ucs2internal;
        break;
      }
      if (sizeof(wchar_t) == 1) {
        to_index = ei_iso8859_1;
        break;
      }
#endif
#if HAVE_MBRTOWC
      to_wchar = 1;
      tocode = locale_charset();
      continue;
#endif
      goto invalid;
    }
    to_index = ap->encoding_index;
    break;
  }
  for (from_wchar = 0;;) {
    /* Search fromcode in the table. */
    for (cp = fromcode, bp = buf, count = MAX_WORD_LENGTH+9+9+1; ; cp++, bp++) {
      unsigned char c = (unsigned char) *cp;
      if (c >= 0x80)
        goto invalid;
      if (c >= 'a' && c <= 'z')
        c -= 'a'-'A';
      *bp = c;
      if (c == '\0')
        break;
      if (--count == 0)
        goto invalid;
    }
    for (;;) {
      char *sp = bp;
      int parsed_translit = 0;
      int parsed_ignore = 0;
      int parsed_non_identical_discard = 0;
      for (;;) {
        if (sp-buf > 9 && memcmp(sp-9,"/TRANSLIT",9)==0) {
          sp = sp - 9;
          parsed_translit = 1;
        } else if (sp-buf > 7 && memcmp(sp-7,"/IGNORE",7)==0) {
          sp = sp - 7;
          parsed_ignore = 1;
        } else if (sp-buf > 22 && memcmp(sp-22,"/NON_IDENTICAL_DISCARD",22)==0) {
          sp = sp - 22;
          parsed_non_identical_discard = 1;
        } else
          break;
        if (sp > buf && sp[-1] == '/')
          sp = sp - 1;
      }
      if (sp > buf && sp[-1] == '/') {
        bp = sp - 1;
      } else if (sp-buf >= 9 && memcmp(sp-9,"/ZOS_UNIX",9)==0) {
        bp = sp - 9;
        from_surface = ICONV_SURFACE_EBCDIC_ZOS_UNIX;
      } else
        bp = sp;
      *bp = '\0';
      if (parsed_translit)
        transliterate = 1;
      if (parsed_ignore)
        discard_ilseq |= DISCARD_INVALID | DISCARD_UNCONVERTIBLE;
      if (parsed_non_identical_discard)
        discard_ilseq |= DISCARD_UNCONVERTIBLE;
      break;
    }
    if (buf[0] == '\0') {
      fromcode = locale_charset();
      /* Avoid an endless loop that could occur when using an older version
         of localcharset.c. */
      if (fromcode[0] == '\0')
        goto invalid;
      continue;
    }
    ap = aliases_lookup(buf,bp-buf);
    if (ap == NULL) {
      ap = aliases2_lookup(buf);
      if (ap == NULL)
        goto invalid;
    }
    if (ap->encoding_index == ei_local_char) {
      fromcode = locale_charset();
      /* Avoid an endless loop that could occur when using an older version
         of localcharset.c. */
      if (fromcode[0] == '\0')
        goto invalid;
      continue;
    }
    if (ap->encoding_index == ei_local_wchar_t) {
      /* On systems which define __STDC_ISO_10646__, wchar_t is Unicode.
         This is also the case on native Woe32 systems and Cygwin >= 1.7, where
         we know that it is UTF-16.  */
#if (defined _WIN32 && !defined __CYGWIN__) || (defined __CYGWIN__ && CYGWIN_VERSION_DLL_MAJOR >= 1007)
      if (sizeof(wchar_t) == 4) {
        from_index = ei_ucs4internal;
        break;
      }
      if (sizeof(wchar_t) == 2) {
# if WORDS_LITTLEENDIAN
        from_index = ei_utf16le;
# else
        from_index = ei_utf16be;
# endif
        break;
      }
#elif __STDC_ISO_10646__
      if (sizeof(wchar_t) == 4) {
        from_index = ei_ucs4internal;
        break;
      }
      if (sizeof(wchar_t) == 2) {
        from_index = ei_ucs2internal;
        break;
      }
      if (sizeof(wchar_t) == 1) {
        from_index = ei_iso8859_1;
        break;
      }
#endif
#if HAVE_WCRTOMB
      from_wchar = 1;
      fromcode = locale_charset();
      continue;
#endif
      goto invalid;
    }
    from_index = ap->encoding_index;
    break;
  }
}
