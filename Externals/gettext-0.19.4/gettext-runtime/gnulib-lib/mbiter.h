/* Iterating through multibyte strings: macros for multi-byte encodings.
   Copyright (C) 2001, 2005, 2007, 2009-2014 Free Software Foundation, Inc.

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

/* Written by Bruno Haible <bruno@clisp.org>.  */

/* The macros in this file implement forward iteration through a
   multi-byte string.

   With these macros, an iteration loop that looks like

      char *iter;
      for (iter = buf; iter < buf + buflen; iter++)
        {
          do_something (*iter);
        }

   becomes

      mbi_iterator_t iter;
      for (mbi_init (iter, buf, buflen); mbi_avail (iter); mbi_advance (iter))
        {
          do_something (mbi_cur_ptr (iter), mb_len (mbi_cur (iter)));
        }

   The benefit of these macros over plain use of mbrtowc is:
   - Handling of invalid multibyte sequences is possible without
     making the code more complicated, while still preserving the
     invalid multibyte sequences.

   mbi_iterator_t
     is a type usable for variable declarations.

   mbi_init (iter, startptr, length)
     initializes the iterator, starting at startptr and crossing length bytes.

   mbi_avail (iter)
     returns true if there are more multibyte characters available before
     the end of string is reached. In this case, mbi_cur (iter) is
     initialized to the next multibyte character.

   mbi_advance (iter)
     advances the iterator by one multibyte character.

   mbi_cur (iter)
     returns the current multibyte character, of type mbchar_t.  All the
     macros defined in mbchar.h can be used on it.

   mbi_cur_ptr (iter)
     return a pointer to the beginning of the current multibyte character.

   mbi_reloc (iter, ptrdiff)
     relocates iterator when the string is moved by ptrdiff bytes.

   mbi_copy (&destiter, &srciter)
     copies srciter to destiter.

   Here are the function prototypes of the macros.

   extern void          mbi_init (mbi_iterator_t iter,
                                  const char *startptr, size_t length);
   extern bool          mbi_avail (mbi_iterator_t iter);
   extern void          mbi_advance (mbi_iterator_t iter);
   extern mbchar_t      mbi_cur (mbi_iterator_t iter);
   extern const char *  mbi_cur_ptr (mbi_iterator_t iter);
   extern void          mbi_reloc (mbi_iterator_t iter, ptrdiff_t ptrdiff);
   extern void          mbi_copy (mbi_iterator_t *new, const mbi_iterator_t *old);
 */

#ifndef _MBITER_H
#define _MBITER_H 1

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* Tru64 with Desktop Toolkit C has a bug: <stdio.h> must be included before
   <wchar.h>.
   BSD/OS 4.1 has a bug: <stdio.h> and <time.h> must be included before
   <wchar.h>.  */
#include <stdio.h>
#include <time.h>
#include <wchar.h>

#include "mbchar.h"

#ifndef _GL_INLINE_HEADER_BEGIN
 #error "Please include config.h first."
#endif
_GL_INLINE_HEADER_BEGIN
#ifndef MBITER_INLINE
# define MBITER_INLINE _GL_INLINE
#endif

struct mbiter_multi
{
  const char *limit;    /* pointer to end of string */
  bool in_shift;        /* true if next byte may not be interpreted as ASCII */
  mbstate_t state;      /* if in_shift: current shift state */
  bool next_done;       /* true if mbi_avail has already filled the following */
  struct mbchar cur;    /* the current character:
        const char *cur.ptr             pointer to current character
        The following are only valid after mbi_avail.
        size_t cur.bytes                number of bytes of current character
        bool cur.wc_valid               true if wc is a valid wide character
        wchar_t cur.wc                  if wc_valid: the current character
        */
};

MBITER_INLINE void
mbiter_multi_next (struct mbiter_multi *iter)
{
  if (iter->next_done)
    return;
  if (iter->in_shift)
    goto with_shift;
  /* Handle most ASCII characters quickly, without calling mbrtowc().  */
  if (is_basic (*iter->cur.ptr))
    {
      /* These characters are part of the basic character set.  ISO C 99
         guarantees that their wide character code is identical to their
         char code.  */
      iter->cur.bytes = 1;
      iter->cur.wc = *iter->cur.ptr;
      iter->cur.wc_valid = true;
    }
  else
    {
      assert (mbsinit (&iter->state));
      iter->in_shift = true;
    with_shift:
      iter->cur.bytes = mbrtowc (&iter->cur.wc, iter->cur.ptr,
                                 iter->limit - iter->cur.ptr, &iter->state);
      if (iter->cur.bytes == (size_t) -1)
        {
          /* An invalid multibyte sequence was encountered.  */
          iter->cur.bytes = 1;
          iter->cur.wc_valid = false;
          /* Whether to set iter->in_shift = false and reset iter->state
             or not is not very important; the string is bogus anyway.  */
        }
      else if (iter->cur.bytes == (size_t) -2)
        {
          /* An incomplete multibyte character at the end.  */
          iter->cur.bytes = iter->limit - iter->cur.ptr;
          iter->cur.wc_valid = false;
          /* Whether to set iter->in_shift = false and reset iter->state
             or not is not important; the string end is reached anyway.  */
        }
      else
        {
          if (iter->cur.bytes == 0)
            {
              /* A null wide character was encountered.  */
              iter->cur.bytes = 1;
              assert (*iter->cur.ptr == '\0');
              assert (iter->cur.wc == 0);
            }
          iter->cur.wc_valid = true;

          /* When in the initial state, we can go back treating ASCII
             characters more quickly.  */
          if (mbsinit (&iter->state))
            iter->in_shift = false;
        }
    }
  iter->next_done = true;
}

MBITER_INLINE void
mbiter_multi_reloc (struct mbiter_multi *iter, ptrdiff_t ptrdiff)
{
  iter->cur.ptr += ptrdiff;
  iter->limit += ptrdiff;
}

MBITER_INLINE void
mbiter_multi_copy (struct mbiter_multi *new_iter, const struct mbiter_multi *old_iter)
{
  new_iter->limit = old_iter->limit;
  if ((new_iter->in_shift = old_iter->in_shift))
    memcpy (&new_iter->state, &old_iter->state, sizeof (mbstate_t));
  else
    memset (&new_iter->state, 0, sizeof (mbstate_t));
  new_iter->next_done = old_iter->next_done;
  mb_copy (&new_iter->cur, &old_iter->cur);
}

/* Iteration macros.  */
typedef struct mbiter_multi mbi_iterator_t;
#define mbi_init(iter, startptr, length) \
  ((iter).cur.ptr = (startptr), (iter).limit = (iter).cur.ptr + (length), \
   (iter).in_shift = false, memset (&(iter).state, '\0', sizeof (mbstate_t)), \
   (iter).next_done = false)
#define mbi_avail(iter) \
  ((iter).cur.ptr < (iter).limit && (mbiter_multi_next (&(iter)), true))
#define mbi_advance(iter) \
  ((iter).cur.ptr += (iter).cur.bytes, (iter).next_done = false)

/* Access to the current character.  */
#define mbi_cur(iter) (iter).cur
#define mbi_cur_ptr(iter) (iter).cur.ptr

/* Relocation.  */
#define mbi_reloc(iter, ptrdiff) mbiter_multi_reloc (&iter, ptrdiff)

/* Copying an iterator.  */
#define mbi_copy mbiter_multi_copy

_GL_INLINE_HEADER_END

#endif /* _MBITER_H */
