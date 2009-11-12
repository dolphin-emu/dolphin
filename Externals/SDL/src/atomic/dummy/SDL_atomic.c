/*
  SDL - Simple DirectMedia Layer
  Copyright (C) 1997-2009 Sam Lantinga

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Sam Lantinga
  slouken@libsdl.org

  Contributed by Bob Pendleton, bob@pendleton.com
*/

#include "SDL_stdinc.h"
#include "SDL_atomic.h"

#include "SDL_error.h"

/*
  This file provides 32, and 64 bit atomic operations. If the
  operations are provided by the native hardware and operating system
  they are used. If they are not then the operations are emulated
  using the SDL spin lock operations. If spin lock can not be
  implemented then these functions must fail.
*/

/* 
  DUMMY VERSION.

  This version of the code assumes there is no support for atomic
  operations. Therefore, every function sets the SDL error
  message. Oddly enough, if you only have one thread then this
  version actuallys works.
*/

/*
  Native spinlock routines. Because this is the dummy implementation
  these will always call SDL_SetError() and do nothing.
*/

void 
SDL_AtomicLock(SDL_SpinLock *lock)
{
   SDL_SetError("SDL_atomic.c: is not implemented on this platform");
}

void 
SDL_AtomicUnlock(SDL_SpinLock *lock)
{
   SDL_SetError("SDL_atomic.c: is not implemented on this platform");
}

/*
  Note that platform specific versions can be built from this version
  by changing the #undefs to #defines and adding platform specific
  code.
*/

#undef  nativeTestThenSet32
#undef  nativeClear32
#undef  nativeFetchThenIncrement32
#undef  nativeFetchThenDecrement32
#undef  nativeFetchThenAdd32
#undef  nativeFetchThenSubtract32
#undef  nativeIncrementThenFetch32
#undef  nativeDecrementThenFetch32
#undef  nativeAddThenFetch32
#undef  nativeSubtractThenFetch32

#undef  nativeTestThenSet64
#undef  nativeClear64
#undef  nativeFetchThenIncrement64
#undef  nativeFetchThenDecrement64
#undef  nativeFetchThenAdd64
#undef  nativeFetchThenSubtract64
#undef  nativeIncrementThenFetch64
#undef  nativeDecrementThenFetch64
#undef  nativeAddThenFetch64
#undef  nativeSubtractThenFetch64

/* 
  If any of the operations are not provided then we must emulate some
  of them. That means we need a nice implementation of spin locks
  that avoids the "one big lock" problem. We use a vector of spin
  locks and pick which one to use based on the address of the operand
  of the function.

  To generate the index of the lock we first shift by 3 bits to get
  rid on the zero bits that result from 32 and 64 bit allignment of
  data. We then mask off all but 5 bits and use those 5 bits as an
  index into the table. 

  Picking the lock this way insures that accesses to the same data at
  the same time will go to the same lock. OTOH, accesses to different
  data have only a 1/32 chance of hitting the same lock. That should
  pretty much eliminate the chances of several atomic operations on
  different data from waiting on the same "big lock". If it isn't
  then the table of locks can be expanded to a new size so long as
  the new size is a power of two.
*/

static SDL_SpinLock locks[32] = {
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
};

static __inline__ void
privateWaitLock(volatile void *ptr)
{
#if SIZEOF_VOIDP == 4
   Uint32 index = ((((Uint32)ptr) >> 3) & 0x1f);
#elif SIZEOF_VOIDP == 8
   Uint64 index = ((((Uint64)ptr) >> 3) & 0x1f);
#endif

   SDL_AtomicLock(&locks[index]);
}

static __inline__ void
privateUnlock(volatile void *ptr)
{
#if SIZEOF_VOIDP == 4
   Uint32 index = ((((Uint32)ptr) >> 3) & 0x1f);
#elif SIZEOF_VOIDP == 8
   Uint64 index = ((((Uint64)ptr) >> 3) & 0x1f);
#endif

   SDL_AtomicUnlock(&locks[index]);
}

/* 32 bit atomic operations */

SDL_bool
SDL_AtomicTestThenSet32(volatile Uint32 * ptr)
{
#ifdef nativeTestThenSet32
#else
   SDL_bool result = SDL_FALSE;

   privateWaitLock(ptr);
   result = (*ptr == 0);
   if (result)
   {
      *ptr = 1;
   }
   privateUnlock(ptr);

   return result;
#endif
}

void
SDL_AtomicClear32(volatile Uint32 * ptr)
{
#ifdef nativeClear32
#else
   privateWaitLock(ptr);
   *ptr = 0;
   privateUnlock(ptr);

   return;
#endif
}

Uint32
SDL_AtomicFetchThenIncrement32(volatile Uint32 * ptr)
{
#ifdef nativeFetchThenIncrement32
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)+= 1;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint32
SDL_AtomicFetchThenDecrement32(volatile Uint32 * ptr)
{
#ifdef nativeFetchThenDecrement32
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr) -= 1;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint32
SDL_AtomicFetchThenAdd32(volatile Uint32 * ptr, Uint32 value)
{
#ifdef nativeFetchThenAdd32
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)+= value;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint32
SDL_AtomicFetchThenSubtract32(volatile Uint32 * ptr, Uint32 value)
{
#ifdef nativeFetchThenSubtract32
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)-= value;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint32
SDL_AtomicIncrementThenFetch32(volatile Uint32 * ptr)
{
#ifdef nativeIncrementThenFetch32
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)+= 1;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint32
SDL_AtomicDecrementThenFetch32(volatile Uint32 * ptr)
{
#ifdef nativeDecrementThenFetch32
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)-= 1;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint32
SDL_AtomicAddThenFetch32(volatile Uint32 * ptr, Uint32 value)
{
#ifdef nativeAddThenFetch32
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)+= value;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint32
SDL_AtomicSubtractThenFetch32(volatile Uint32 * ptr, Uint32 value)
{
#ifdef nativeSubtractThenFetch32
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)-= value;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif
}

/* 64 bit atomic operations */
#ifdef SDL_HAS_64BIT_TYPE

SDL_bool
SDL_AtomicTestThenSet64(volatile Uint64 * ptr)
{
#ifdef nativeTestThenSet64
#else
   SDL_bool result = SDL_FALSE;

   privateWaitLock(ptr);
   result = (*ptr == 0);
   if (result)
   {
      *ptr = 1;
   }
   privateUnlock(ptr);

   return result;
#endif
}

void
SDL_AtomicClear64(volatile Uint64 * ptr)
{
#ifdef nativeClear64
#else
   privateWaitLock(ptr);
   *ptr = 0;
   privateUnlock(ptr);

   return;
#endif
}

Uint64
SDL_AtomicFetchThenIncrement64(volatile Uint64 * ptr)
{
#ifdef nativeFetchThenIncrement64
#else
   Uint64 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)+= 1;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint64
SDL_AtomicFetchThenDecrement64(volatile Uint64 * ptr)
{
#ifdef nativeFetchThenDecrement64
#else
   Uint64 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr) -= 1;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint64
SDL_AtomicFetchThenAdd64(volatile Uint64 * ptr, Uint64 value)
{
#ifdef nativeFetchThenAdd64
#else
   Uint64 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)+= value;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint64
SDL_AtomicFetchThenSubtract64(volatile Uint64 * ptr, Uint64 value)
{
#ifdef nativeFetchThenSubtract64
#else
   Uint64 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)-= value;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint64
SDL_AtomicIncrementThenFetch64(volatile Uint64 * ptr)
{
#ifdef nativeIncrementThenFetch64
#else
   Uint64 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)+= 1;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint64
SDL_AtomicDecrementThenFetch64(volatile Uint64 * ptr)
{
#ifdef nativeDecrementThenFetch64
#else
   Uint64 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)-= 1;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint64
SDL_AtomicAddThenFetch64(volatile Uint64 * ptr, Uint64 value)
{
#ifdef nativeAddThenFetch64
#else
   Uint64 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)+= value;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif
}

Uint64
SDL_AtomicSubtractThenFetch64(volatile Uint64 * ptr, Uint64 value)
{
#ifdef nativeSubtractThenFetch64
#else
   Uint64 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)-= value;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif
}
#endif

