/* Elementary Unicode string functions.
   Copyright (C) 2001-2002, 2005-2014 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef _UNISTR_H
#define _UNISTR_H

#include "unitypes.h"

/* Get common macros for C.  */
#include "unused-parameter.h"

/* Get bool.  */
#include <stdbool.h>

/* Get size_t.  */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Conventions:

   All functions prefixed with u8_ operate on UTF-8 encoded strings.
   Their unit is an uint8_t (1 byte).

   All functions prefixed with u16_ operate on UTF-16 encoded strings.
   Their unit is an uint16_t (a 2-byte word).

   All functions prefixed with u32_ operate on UCS-4 encoded strings.
   Their unit is an uint32_t (a 4-byte word).

   All argument pairs (s, n) denote a Unicode string s[0..n-1] with exactly
   n units.

   All arguments starting with "str" and the arguments of functions starting
   with u8_str/u16_str/u32_str denote a NUL terminated string, i.e. a string
   which terminates at the first NUL unit.  This termination unit is
   considered part of the string for all memory allocation purposes, but
   is not considered part of the string for all other logical purposes.

   Functions returning a string result take a (resultbuf, lengthp) argument
   pair.  If resultbuf is not NULL and the result fits into *lengthp units,
   it is put in resultbuf, and resultbuf is returned.  Otherwise, a freshly
   allocated string is returned.  In both cases, *lengthp is set to the
   length (number of units) of the returned string.  In case of error,
   NULL is returned and errno is set.  */


/* Elementary string checks.  */

/* Check whether an UTF-8 string is well-formed.
   Return NULL if valid, or a pointer to the first invalid unit otherwise.  */
extern const uint8_t *
       u8_check (const uint8_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;

/* Check whether an UTF-16 string is well-formed.
   Return NULL if valid, or a pointer to the first invalid unit otherwise.  */
extern const uint16_t *
       u16_check (const uint16_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;

/* Check whether an UCS-4 string is well-formed.
   Return NULL if valid, or a pointer to the first invalid unit otherwise.  */
extern const uint32_t *
       u32_check (const uint32_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;


/* Elementary string conversions.  */

/* Convert an UTF-8 string to an UTF-16 string.  */
extern uint16_t *
       u8_to_u16 (const uint8_t *s, size_t n, uint16_t *resultbuf,
                  size_t *lengthp);

/* Convert an UTF-8 string to an UCS-4 string.  */
extern uint32_t *
       u8_to_u32 (const uint8_t *s, size_t n, uint32_t *resultbuf,
                  size_t *lengthp);

/* Convert an UTF-16 string to an UTF-8 string.  */
extern uint8_t *
       u16_to_u8 (const uint16_t *s, size_t n, uint8_t *resultbuf,
                  size_t *lengthp);

/* Convert an UTF-16 string to an UCS-4 string.  */
extern uint32_t *
       u16_to_u32 (const uint16_t *s, size_t n, uint32_t *resultbuf,
                   size_t *lengthp);

/* Convert an UCS-4 string to an UTF-8 string.  */
extern uint8_t *
       u32_to_u8 (const uint32_t *s, size_t n, uint8_t *resultbuf,
                  size_t *lengthp);

/* Convert an UCS-4 string to an UTF-16 string.  */
extern uint16_t *
       u32_to_u16 (const uint32_t *s, size_t n, uint16_t *resultbuf,
                   size_t *lengthp);


/* Elementary string functions.  */

/* Return the length (number of units) of the first character in S, which is
   no longer than N.  Return 0 if it is the NUL character.  Return -1 upon
   failure.  */
/* Similar to mblen(), except that s must not be NULL.  */
extern int
       u8_mblen (const uint8_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_mblen (const uint16_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_mblen (const uint32_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;

/* Return the length (number of units) of the first character in S, putting
   its 'ucs4_t' representation in *PUC.  Upon failure, *PUC is set to 0xfffd,
   and an appropriate number of units is returned.
   The number of available units, N, must be > 0.  */
/* Similar to mbtowc(), except that puc and s must not be NULL, n must be > 0,
   and the NUL character is not treated specially.  */
/* The variants with _safe suffix are safe, even if the library is compiled
   without --enable-safety.  */

#if GNULIB_UNISTR_U8_MBTOUC_UNSAFE || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u8_mbtouc_unsafe (ucs4_t *puc, const uint8_t *s, size_t n);
# else
extern int
       u8_mbtouc_unsafe_aux (ucs4_t *puc, const uint8_t *s, size_t n);
static inline int
u8_mbtouc_unsafe (ucs4_t *puc, const uint8_t *s, size_t n)
{
  uint8_t c = *s;

  if (c < 0x80)
    {
      *puc = c;
      return 1;
    }
  else
    return u8_mbtouc_unsafe_aux (puc, s, n);
}
# endif
#endif

#if GNULIB_UNISTR_U16_MBTOUC_UNSAFE || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u16_mbtouc_unsafe (ucs4_t *puc, const uint16_t *s, size_t n);
# else
extern int
       u16_mbtouc_unsafe_aux (ucs4_t *puc, const uint16_t *s, size_t n);
static inline int
u16_mbtouc_unsafe (ucs4_t *puc, const uint16_t *s, size_t n)
{
  uint16_t c = *s;

  if (c < 0xd800 || c >= 0xe000)
    {
      *puc = c;
      return 1;
    }
  else
    return u16_mbtouc_unsafe_aux (puc, s, n);
}
# endif
#endif

#if GNULIB_UNISTR_U32_MBTOUC_UNSAFE || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u32_mbtouc_unsafe (ucs4_t *puc, const uint32_t *s, size_t n);
# else
static inline int
u32_mbtouc_unsafe (ucs4_t *puc,
                   const uint32_t *s, size_t n _GL_UNUSED_PARAMETER)
{
  uint32_t c = *s;

#  if CONFIG_UNICODE_SAFETY
  if (c < 0xd800 || (c >= 0xe000 && c < 0x110000))
#  endif
    *puc = c;
#  if CONFIG_UNICODE_SAFETY
  else
    /* invalid multibyte character */
    *puc = 0xfffd;
#  endif
  return 1;
}
# endif
#endif

#if GNULIB_UNISTR_U8_MBTOUC || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u8_mbtouc (ucs4_t *puc, const uint8_t *s, size_t n);
# else
extern int
       u8_mbtouc_aux (ucs4_t *puc, const uint8_t *s, size_t n);
static inline int
u8_mbtouc (ucs4_t *puc, const uint8_t *s, size_t n)
{
  uint8_t c = *s;

  if (c < 0x80)
    {
      *puc = c;
      return 1;
    }
  else
    return u8_mbtouc_aux (puc, s, n);
}
# endif
#endif

#if GNULIB_UNISTR_U16_MBTOUC || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u16_mbtouc (ucs4_t *puc, const uint16_t *s, size_t n);
# else
extern int
       u16_mbtouc_aux (ucs4_t *puc, const uint16_t *s, size_t n);
static inline int
u16_mbtouc (ucs4_t *puc, const uint16_t *s, size_t n)
{
  uint16_t c = *s;

  if (c < 0xd800 || c >= 0xe000)
    {
      *puc = c;
      return 1;
    }
  else
    return u16_mbtouc_aux (puc, s, n);
}
# endif
#endif

#if GNULIB_UNISTR_U32_MBTOUC || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u32_mbtouc (ucs4_t *puc, const uint32_t *s, size_t n);
# else
static inline int
u32_mbtouc (ucs4_t *puc, const uint32_t *s, size_t n _GL_UNUSED_PARAMETER)
{
  uint32_t c = *s;

  if (c < 0xd800 || (c >= 0xe000 && c < 0x110000))
    *puc = c;
  else
    /* invalid multibyte character */
    *puc = 0xfffd;
  return 1;
}
# endif
#endif

/* Return the length (number of units) of the first character in S, putting
   its 'ucs4_t' representation in *PUC.  Upon failure, *PUC is set to 0xfffd,
   and -1 is returned for an invalid sequence of units, -2 is returned for an
   incomplete sequence of units.
   The number of available units, N, must be > 0.  */
/* Similar to u*_mbtouc(), except that the return value gives more details
   about the failure, similar to mbrtowc().  */

#if GNULIB_UNISTR_U8_MBTOUCR || HAVE_LIBUNISTRING
extern int
       u8_mbtoucr (ucs4_t *puc, const uint8_t *s, size_t n);
#endif

#if GNULIB_UNISTR_U16_MBTOUCR || HAVE_LIBUNISTRING
extern int
       u16_mbtoucr (ucs4_t *puc, const uint16_t *s, size_t n);
#endif

#if GNULIB_UNISTR_U32_MBTOUCR || HAVE_LIBUNISTRING
extern int
       u32_mbtoucr (ucs4_t *puc, const uint32_t *s, size_t n);
#endif

/* Put the multibyte character represented by UC in S, returning its
   length.  Return -1 upon failure, -2 if the number of available units, N,
   is too small.  The latter case cannot occur if N >= 6/2/1, respectively.  */
/* Similar to wctomb(), except that s must not be NULL, and the argument n
   must be specified.  */

#if GNULIB_UNISTR_U8_UCTOMB || HAVE_LIBUNISTRING
/* Auxiliary function, also used by u8_chr, u8_strchr, u8_strrchr.  */
extern int
       u8_uctomb_aux (uint8_t *s, ucs4_t uc, int n);
# if !HAVE_INLINE
extern int
       u8_uctomb (uint8_t *s, ucs4_t uc, int n);
# else
static inline int
u8_uctomb (uint8_t *s, ucs4_t uc, int n)
{
  if (uc < 0x80 && n > 0)
    {
      s[0] = uc;
      return 1;
    }
  else
    return u8_uctomb_aux (s, uc, n);
}
# endif
#endif

#if GNULIB_UNISTR_U16_UCTOMB || HAVE_LIBUNISTRING
/* Auxiliary function, also used by u16_chr, u16_strchr, u16_strrchr.  */
extern int
       u16_uctomb_aux (uint16_t *s, ucs4_t uc, int n);
# if !HAVE_INLINE
extern int
       u16_uctomb (uint16_t *s, ucs4_t uc, int n);
# else
static inline int
u16_uctomb (uint16_t *s, ucs4_t uc, int n)
{
  if (uc < 0xd800 && n > 0)
    {
      s[0] = uc;
      return 1;
    }
  else
    return u16_uctomb_aux (s, uc, n);
}
# endif
#endif

#if GNULIB_UNISTR_U32_UCTOMB || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u32_uctomb (uint32_t *s, ucs4_t uc, int n);
# else
static inline int
u32_uctomb (uint32_t *s, ucs4_t uc, int n)
{
  if (uc < 0xd800 || (uc >= 0xe000 && uc < 0x110000))
    {
      if (n > 0)
        {
          *s = uc;
          return 1;
        }
      else
        return -2;
    }
  else
    return -1;
}
# endif
#endif

/* Copy N units from SRC to DEST.  */
/* Similar to memcpy().  */
extern uint8_t *
       u8_cpy (uint8_t *dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_cpy (uint16_t *dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_cpy (uint32_t *dest, const uint32_t *src, size_t n);

/* Copy N units from SRC to DEST, guaranteeing correct behavior for
   overlapping memory areas.  */
/* Similar to memmove().  */
extern uint8_t *
       u8_move (uint8_t *dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_move (uint16_t *dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_move (uint32_t *dest, const uint32_t *src, size_t n);

/* Set the first N characters of S to UC.  UC should be a character that
   occupies only 1 unit.  */
/* Similar to memset().  */
extern uint8_t *
       u8_set (uint8_t *s, ucs4_t uc, size_t n);
extern uint16_t *
       u16_set (uint16_t *s, ucs4_t uc, size_t n);
extern uint32_t *
       u32_set (uint32_t *s, ucs4_t uc, size_t n);

/* Compare S1 and S2, each of length N.  */
/* Similar to memcmp().  */
extern int
       u8_cmp (const uint8_t *s1, const uint8_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_cmp (const uint16_t *s1, const uint16_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_cmp (const uint32_t *s1, const uint32_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;

/* Compare S1 and S2.  */
/* Similar to the gnulib function memcmp2().  */
extern int
       u8_cmp2 (const uint8_t *s1, size_t n1, const uint8_t *s2, size_t n2)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_cmp2 (const uint16_t *s1, size_t n1, const uint16_t *s2, size_t n2)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_cmp2 (const uint32_t *s1, size_t n1, const uint32_t *s2, size_t n2)
       _UC_ATTRIBUTE_PURE;

/* Search the string at S for UC.  */
/* Similar to memchr().  */
extern uint8_t *
       u8_chr (const uint8_t *s, size_t n, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_chr (const uint16_t *s, size_t n, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_chr (const uint32_t *s, size_t n, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;

/* Count the number of Unicode characters in the N units from S.  */
/* Similar to mbsnlen().  */
extern size_t
       u8_mbsnlen (const uint8_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_mbsnlen (const uint16_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_mbsnlen (const uint32_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;

/* Elementary string functions with memory allocation.  */

/* Make a freshly allocated copy of S, of length N.  */
extern uint8_t *
       u8_cpy_alloc (const uint8_t *s, size_t n);
extern uint16_t *
       u16_cpy_alloc (const uint16_t *s, size_t n);
extern uint32_t *
       u32_cpy_alloc (const uint32_t *s, size_t n);

/* Elementary string functions on NUL terminated strings.  */

/* Return the length (number of units) of the first character in S.
   Return 0 if it is the NUL character.  Return -1 upon failure.  */
extern int
       u8_strmblen (const uint8_t *s)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_strmblen (const uint16_t *s)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_strmblen (const uint32_t *s)
       _UC_ATTRIBUTE_PURE;

/* Return the length (number of units) of the first character in S, putting
   its 'ucs4_t' representation in *PUC.  Return 0 if it is the NUL
   character.  Return -1 upon failure.  */
extern int
       u8_strmbtouc (ucs4_t *puc, const uint8_t *s);
extern int
       u16_strmbtouc (ucs4_t *puc, const uint16_t *s);
extern int
       u32_strmbtouc (ucs4_t *puc, const uint32_t *s);

/* Forward iteration step.  Advances the pointer past the next character,
   or returns NULL if the end of the string has been reached.  Puts the
   character's 'ucs4_t' representation in *PUC.  */
extern const uint8_t *
       u8_next (ucs4_t *puc, const uint8_t *s);
extern const uint16_t *
       u16_next (ucs4_t *puc, const uint16_t *s);
extern const uint32_t *
       u32_next (ucs4_t *puc, const uint32_t *s);

/* Backward iteration step.  Advances the pointer to point to the previous
   character, or returns NULL if the beginning of the string had been reached.
   Puts the character's 'ucs4_t' representation in *PUC.  */
extern const uint8_t *
       u8_prev (ucs4_t *puc, const uint8_t *s, const uint8_t *start);
extern const uint16_t *
       u16_prev (ucs4_t *puc, const uint16_t *s, const uint16_t *start);
extern const uint32_t *
       u32_prev (ucs4_t *puc, const uint32_t *s, const uint32_t *start);

/* Return the number of units in S.  */
/* Similar to strlen(), wcslen().  */
extern size_t
       u8_strlen (const uint8_t *s)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_strlen (const uint16_t *s)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_strlen (const uint32_t *s)
       _UC_ATTRIBUTE_PURE;

/* Return the number of units in S, but at most MAXLEN.  */
/* Similar to strnlen(), wcsnlen().  */
extern size_t
       u8_strnlen (const uint8_t *s, size_t maxlen)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_strnlen (const uint16_t *s, size_t maxlen)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_strnlen (const uint32_t *s, size_t maxlen)
       _UC_ATTRIBUTE_PURE;

/* Copy SRC to DEST.  */
/* Similar to strcpy(), wcscpy().  */
extern uint8_t *
       u8_strcpy (uint8_t *dest, const uint8_t *src);
extern uint16_t *
       u16_strcpy (uint16_t *dest, const uint16_t *src);
extern uint32_t *
       u32_strcpy (uint32_t *dest, const uint32_t *src);

/* Copy SRC to DEST, returning the address of the terminating NUL in DEST.  */
/* Similar to stpcpy().  */
extern uint8_t *
       u8_stpcpy (uint8_t *dest, const uint8_t *src);
extern uint16_t *
       u16_stpcpy (uint16_t *dest, const uint16_t *src);
extern uint32_t *
       u32_stpcpy (uint32_t *dest, const uint32_t *src);

/* Copy no more than N units of SRC to DEST.  */
/* Similar to strncpy(), wcsncpy().  */
extern uint8_t *
       u8_strncpy (uint8_t *dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_strncpy (uint16_t *dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_strncpy (uint32_t *dest, const uint32_t *src, size_t n);

/* Copy no more than N units of SRC to DEST.  Return a pointer past the last
   non-NUL unit written into DEST.  */
/* Similar to stpncpy().  */
extern uint8_t *
       u8_stpncpy (uint8_t *dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_stpncpy (uint16_t *dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_stpncpy (uint32_t *dest, const uint32_t *src, size_t n);

/* Append SRC onto DEST.  */
/* Similar to strcat(), wcscat().  */
extern uint8_t *
       u8_strcat (uint8_t *dest, const uint8_t *src);
extern uint16_t *
       u16_strcat (uint16_t *dest, const uint16_t *src);
extern uint32_t *
       u32_strcat (uint32_t *dest, const uint32_t *src);

/* Append no more than N units of SRC onto DEST.  */
/* Similar to strncat(), wcsncat().  */
extern uint8_t *
       u8_strncat (uint8_t *dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_strncat (uint16_t *dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_strncat (uint32_t *dest, const uint32_t *src, size_t n);

/* Compare S1 and S2.  */
/* Similar to strcmp(), wcscmp().  */
#ifdef __sun
/* Avoid a collision with the u8_strcmp() function in Solaris 11 libc.  */
extern int
       u8_strcmp_gnu (const uint8_t *s1, const uint8_t *s2)
       _UC_ATTRIBUTE_PURE;
# define u8_strcmp u8_strcmp_gnu
#else
extern int
       u8_strcmp (const uint8_t *s1, const uint8_t *s2)
       _UC_ATTRIBUTE_PURE;
#endif
extern int
       u16_strcmp (const uint16_t *s1, const uint16_t *s2)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_strcmp (const uint32_t *s1, const uint32_t *s2)
       _UC_ATTRIBUTE_PURE;

/* Compare S1 and S2 using the collation rules of the current locale.
   Return -1 if S1 < S2, 0 if S1 = S2, 1 if S1 > S2.
   Upon failure, set errno and return any value.  */
/* Similar to strcoll(), wcscoll().  */
extern int
       u8_strcoll (const uint8_t *s1, const uint8_t *s2);
extern int
       u16_strcoll (const uint16_t *s1, const uint16_t *s2);
extern int
       u32_strcoll (const uint32_t *s1, const uint32_t *s2);

/* Compare no more than N units of S1 and S2.  */
/* Similar to strncmp(), wcsncmp().  */
extern int
       u8_strncmp (const uint8_t *s1, const uint8_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_strncmp (const uint16_t *s1, const uint16_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_strncmp (const uint32_t *s1, const uint32_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;

/* Duplicate S, returning an identical malloc'd string.  */
/* Similar to strdup(), wcsdup().  */
extern uint8_t *
       u8_strdup (const uint8_t *s);
extern uint16_t *
       u16_strdup (const uint16_t *s);
extern uint32_t *
       u32_strdup (const uint32_t *s);

/* Find the first occurrence of UC in STR.  */
/* Similar to strchr(), wcschr().  */
extern uint8_t *
       u8_strchr (const uint8_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_strchr (const uint16_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_strchr (const uint32_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;

/* Find the last occurrence of UC in STR.  */
/* Similar to strrchr(), wcsrchr().  */
extern uint8_t *
       u8_strrchr (const uint8_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_strrchr (const uint16_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_strrchr (const uint32_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;

/* Return the length of the initial segment of STR which consists entirely
   of Unicode characters not in REJECT.  */
/* Similar to strcspn(), wcscspn().  */
extern size_t
       u8_strcspn (const uint8_t *str, const uint8_t *reject)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_strcspn (const uint16_t *str, const uint16_t *reject)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_strcspn (const uint32_t *str, const uint32_t *reject)
       _UC_ATTRIBUTE_PURE;

/* Return the length of the initial segment of STR which consists entirely
   of Unicode characters in ACCEPT.  */
/* Similar to strspn(), wcsspn().  */
extern size_t
       u8_strspn (const uint8_t *str, const uint8_t *accept)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_strspn (const uint16_t *str, const uint16_t *accept)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_strspn (const uint32_t *str, const uint32_t *accept)
       _UC_ATTRIBUTE_PURE;

/* Find the first occurrence in STR of any character in ACCEPT.  */
/* Similar to strpbrk(), wcspbrk().  */
extern uint8_t *
       u8_strpbrk (const uint8_t *str, const uint8_t *accept)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_strpbrk (const uint16_t *str, const uint16_t *accept)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_strpbrk (const uint32_t *str, const uint32_t *accept)
       _UC_ATTRIBUTE_PURE;

/* Find the first occurrence of NEEDLE in HAYSTACK.  */
/* Similar to strstr(), wcsstr().  */
extern uint8_t *
       u8_strstr (const uint8_t *haystack, const uint8_t *needle)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_strstr (const uint16_t *haystack, const uint16_t *needle)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_strstr (const uint32_t *haystack, const uint32_t *needle)
       _UC_ATTRIBUTE_PURE;

/* Test whether STR starts with PREFIX.  */
extern bool
       u8_startswith (const uint8_t *str, const uint8_t *prefix)
       _UC_ATTRIBUTE_PURE;
extern bool
       u16_startswith (const uint16_t *str, const uint16_t *prefix)
       _UC_ATTRIBUTE_PURE;
extern bool
       u32_startswith (const uint32_t *str, const uint32_t *prefix)
       _UC_ATTRIBUTE_PURE;

/* Test whether STR ends with SUFFIX.  */
extern bool
       u8_endswith (const uint8_t *str, const uint8_t *suffix)
       _UC_ATTRIBUTE_PURE;
extern bool
       u16_endswith (const uint16_t *str, const uint16_t *suffix)
       _UC_ATTRIBUTE_PURE;
extern bool
       u32_endswith (const uint32_t *str, const uint32_t *suffix)
       _UC_ATTRIBUTE_PURE;

/* Divide STR into tokens separated by characters in DELIM.
   This interface is actually more similar to wcstok than to strtok.  */
/* Similar to strtok_r(), wcstok().  */
extern uint8_t *
       u8_strtok (uint8_t *str, const uint8_t *delim, uint8_t **ptr);
extern uint16_t *
       u16_strtok (uint16_t *str, const uint16_t *delim, uint16_t **ptr);
extern uint32_t *
       u32_strtok (uint32_t *str, const uint32_t *delim, uint32_t **ptr);


#ifdef __cplusplus
}
#endif

#endif /* _UNISTR_H */
