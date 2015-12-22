/* A POSIX <locale.h>.
   Copyright (C) 2007-2014 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

#ifdef _GL_ALREADY_INCLUDING_LOCALE_H

/* Special invocation conventions to handle Solaris header files
   (through Solaris 10) when combined with gettext's libintl.h.  */

#@INCLUDE_NEXT@ @NEXT_LOCALE_H@

#else
/* Normal invocation convention.  */

#ifndef _@GUARD_PREFIX@_LOCALE_H

#define _GL_ALREADY_INCLUDING_LOCALE_H

/* The include_next requires a split double-inclusion guard.  */
#@INCLUDE_NEXT@ @NEXT_LOCALE_H@

#undef _GL_ALREADY_INCLUDING_LOCALE_H

#ifndef _@GUARD_PREFIX@_LOCALE_H
#define _@GUARD_PREFIX@_LOCALE_H

/* NetBSD 5.0 mis-defines NULL.  */
#include <stddef.h>

/* Mac OS X 10.5 defines the locale_t type in <xlocale.h>.  */
#if @HAVE_XLOCALE_H@
# include <xlocale.h>
#endif

/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_ARG_NONNULL is copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */

/* The LC_MESSAGES locale category is specified in POSIX, but not in ISO C.
   On systems that don't define it, use the same value as GNU libintl.  */
#if !defined LC_MESSAGES
# define LC_MESSAGES 1729
#endif

/* Bionic libc's 'struct lconv' is just a dummy.  */
#if @REPLACE_STRUCT_LCONV@
# define lconv rpl_lconv
struct lconv
{
  /* All 'char *' are actually 'const char *'.  */

  /* Members that depend on the LC_NUMERIC category of the locale.  See
     <http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html#tag_07_03_04> */

  /* Symbol used as decimal point.  */
  char *decimal_point;
  /* Symbol used to separate groups of digits to the left of the decimal
     point.  */
  char *thousands_sep;
  /* Definition of the size of groups of digits to the left of the decimal
     point.  */
  char *grouping;

  /* Members that depend on the LC_MONETARY category of the locale.  See
     <http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html#tag_07_03_03> */

  /* Symbol used as decimal point.  */
  char *mon_decimal_point;
  /* Symbol used to separate groups of digits to the left of the decimal
     point.  */
  char *mon_thousands_sep;
  /* Definition of the size of groups of digits to the left of the decimal
     point.  */
  char *mon_grouping;
  /* Sign used to indicate a value >= 0.  */
  char *positive_sign;
  /* Sign used to indicate a value < 0.  */
  char *negative_sign;

  /* For formatting local currency.  */
  /* Currency symbol (3 characters) followed by separator (1 character).  */
  char *currency_symbol;
  /* Number of digits after the decimal point.  */
  char frac_digits;
  /* For values >= 0: 1 if the currency symbol precedes the number, 0 if it
     comes after the number.  */
  char p_cs_precedes;
  /* For values >= 0: Position of the sign.  */
  char p_sign_posn;
  /* For values >= 0: Placement of spaces between currency symbol, sign, and
     number.  */
  char p_sep_by_space;
  /* For values < 0: 1 if the currency symbol precedes the number, 0 if it
     comes after the number.  */
  char n_cs_precedes;
  /* For values < 0: Position of the sign.  */
  char n_sign_posn;
  /* For values < 0: Placement of spaces between currency symbol, sign, and
     number.  */
  char n_sep_by_space;

  /* For formatting international currency.  */
  /* Currency symbol (3 characters) followed by separator (1 character).  */
  char *int_curr_symbol;
  /* Number of digits after the decimal point.  */
  char int_frac_digits;
  /* For values >= 0: 1 if the currency symbol precedes the number, 0 if it
     comes after the number.  */
  char int_p_cs_precedes;
  /* For values >= 0: Position of the sign.  */
  char int_p_sign_posn;
  /* For values >= 0: Placement of spaces between currency symbol, sign, and
     number.  */
  char int_p_sep_by_space;
  /* For values < 0: 1 if the currency symbol precedes the number, 0 if it
     comes after the number.  */
  char int_n_cs_precedes;
  /* For values < 0: Position of the sign.  */
  char int_n_sign_posn;
  /* For values < 0: Placement of spaces between currency symbol, sign, and
     number.  */
  char int_n_sep_by_space;
};
#endif

#if @GNULIB_LOCALECONV@
# if @REPLACE_LOCALECONV@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef localeconv
#   define localeconv rpl_localeconv
#  endif
_GL_FUNCDECL_RPL (localeconv, struct lconv *, (void));
_GL_CXXALIAS_RPL (localeconv, struct lconv *, (void));
# else
_GL_CXXALIAS_SYS (localeconv, struct lconv *, (void));
# endif
_GL_CXXALIASWARN (localeconv);
#elif @REPLACE_STRUCT_LCONV@
# undef localeconv
# define localeconv localeconv_used_without_requesting_gnulib_module_localeconv
#elif defined GNULIB_POSIXCHECK
# undef localeconv
# if HAVE_RAW_DECL_LOCALECONV
_GL_WARN_ON_USE (localeconv,
                 "localeconv returns too few information on some platforms - "
                 "use gnulib module localeconv for portability");
# endif
#endif

#if @GNULIB_SETLOCALE@
# if @REPLACE_SETLOCALE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef setlocale
#   define setlocale rpl_setlocale
#   define GNULIB_defined_setlocale 1
#  endif
_GL_FUNCDECL_RPL (setlocale, char *, (int category, const char *locale));
_GL_CXXALIAS_RPL (setlocale, char *, (int category, const char *locale));
# else
_GL_CXXALIAS_SYS (setlocale, char *, (int category, const char *locale));
# endif
_GL_CXXALIASWARN (setlocale);
#elif defined GNULIB_POSIXCHECK
# undef setlocale
# if HAVE_RAW_DECL_SETLOCALE
_GL_WARN_ON_USE (setlocale, "setlocale works differently on native Windows - "
                 "use gnulib module setlocale for portability");
# endif
#endif

#if @GNULIB_DUPLOCALE@
# if @REPLACE_DUPLOCALE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef duplocale
#   define duplocale rpl_duplocale
#  endif
_GL_FUNCDECL_RPL (duplocale, locale_t, (locale_t locale) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (duplocale, locale_t, (locale_t locale));
# else
#  if @HAVE_DUPLOCALE@
_GL_CXXALIAS_SYS (duplocale, locale_t, (locale_t locale));
#  endif
# endif
# if @HAVE_DUPLOCALE@
_GL_CXXALIASWARN (duplocale);
# endif
#elif defined GNULIB_POSIXCHECK
# undef duplocale
# if HAVE_RAW_DECL_DUPLOCALE
_GL_WARN_ON_USE (duplocale, "duplocale is buggy on some glibc systems - "
                 "use gnulib module duplocale for portability");
# endif
#endif

#endif /* _@GUARD_PREFIX@_LOCALE_H */
#endif /* ! _GL_ALREADY_INCLUDING_LOCALE_H */
#endif /* _@GUARD_PREFIX@_LOCALE_H */
