/* Memory allocators such as malloc+free.

   Copyright (C) 2011-2014 Free Software Foundation, Inc.

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

/* Written by Paul Eggert.  */

#ifndef _GL_ALLOCATOR_H
#define _GL_ALLOCATOR_H

#include <stddef.h>

/* An object describing a memory allocator family.  */

struct allocator
{
  /* Do not use GCC attributes such as __attribute__ ((malloc)) with
     the function types pointed at by these members, because these
     attributes do not work with pointers to functions.  See
     <http://lists.gnu.org/archive/html/bug-gnulib/2011-04/msg00007.html>.  */

  /* Call ALLOCATE to allocate memory, like 'malloc'.  On failure ALLOCATE
     should return NULL, though not necessarily set errno.  When given
     a zero size it may return NULL even if successful.  */
  void *(*allocate) (size_t);

  /* If nonnull, call REALLOCATE to reallocate memory, like 'realloc'.
     On failure REALLOCATE should return NULL, though not necessarily set
     errno.  When given a zero size it may return NULL even if
     successful.  */
  void *(*reallocate) (void *, size_t);

  /* Call FREE to free memory, like 'free'.  */
  void (*free) (void *);

  /* If nonnull, call DIE (SIZE) if MALLOC (SIZE) or REALLOC (...,
     SIZE) fails.  DIE should not return.  SIZE should equal SIZE_MAX
     if size_t overflow was detected while calculating sizes to be
     passed to MALLOC or REALLOC.  */
  void (*die) (size_t);
};

/* An allocator using the stdlib functions and a null DIE function.  */
extern struct allocator const stdlib_allocator;

#endif /* _GL_ALLOCATOR_H */
