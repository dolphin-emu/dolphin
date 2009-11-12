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

    QNX native atomic operations
    Copyright (C) 2009 Mike Gorchak
    (mike@malva.ua, lestat@i.com.ua)
*/

#include "SDL_stdinc.h"
#include "SDL_atomic.h"
#include "SDL_error.h"

#include <atomic.h>

/* SMP Exchange for PPC platform */
#ifdef __PPC__
#include <ppc/smpxchg.h>
#endif /* __PPC__ */

/* SMP Exchange for ARM platform */
#ifdef __ARM__
#include <arm/smpxchg.h>
#endif /* __ARM__ */

/* SMP Exchange for MIPS platform */
#if defined (__MIPSEB__) || defined(__MIPSEL__)
#include <mips/smpxchg.h>
#endif /* __MIPSEB__ || __MIPSEL__ */

/* SMP Exchange for SH platform */
#ifdef __SH__
#include <sh/smpxchg.h>
#endif /* __SH__ */

/* SMP Exchange for x86 platform */
#ifdef __X86__
#include <x86/smpxchg.h>
#endif /* __X86__ */

/*
  This file provides 32, and 64 bit atomic operations. If the
  operations are provided by the native hardware and operating system
  they are used. If they are not then the operations are emulated
  using the SDL spin lock operations. If spin lock can not be
  implemented then these functions must fail.
*/

void 
SDL_AtomicLock(SDL_SpinLock *lock)
{
   unsigned volatile* l = (unsigned volatile*)lock;
   Uint32 oldval = 0;
   Uint32 newval = 1;

   oldval = _smp_xchg(l, newval);
   while(1 == oldval)
   {
      oldval = _smp_xchg(l, newval);
   }
}

void 
SDL_AtomicUnlock(SDL_SpinLock *lock)
{
   unsigned volatile* l = (unsigned volatile*)lock;
   Uint32 newval = 0;

   _smp_xchg(l, newval);
}

/*
   QNX 6.4.1 supports only 32 bit atomic access
*/

#undef   nativeTestThenSet32
#define  nativeClear32
#define  nativeFetchThenIncrement32
#define  nativeFetchThenDecrement32
#define  nativeFetchThenAdd32
#define  nativeFetchThenSubtract32
#define  nativeIncrementThenFetch32
#define  nativeDecrementThenFetch32
#define  nativeAddThenFetch32
#define  nativeSubtractThenFetch32

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
#endif /* SIZEOF_VOIDP */

   SDL_AtomicLock(&locks[index]);
}

static __inline__ void
privateUnlock(volatile void *ptr)
{
#if SIZEOF_VOIDP == 4
   Uint32 index = ((((Uint32)ptr) >> 3) & 0x1f);
#elif SIZEOF_VOIDP == 8
   Uint64 index = ((((Uint64)ptr) >> 3) & 0x1f);
#endif /* SIZEOF_VOIDP */

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
#endif /* nativeTestThenSet32 */
}

void
SDL_AtomicClear32(volatile Uint32 * ptr)
{
#ifdef nativeClear32
   atomic_clr(ptr, 0xFFFFFFFF);
#else
   privateWaitLock(ptr);
   *ptr = 0;
   privateUnlock(ptr);

   return;
#endif /* nativeClear32 */
}

Uint32
SDL_AtomicFetchThenIncrement32(volatile Uint32 * ptr)
{
#ifdef nativeFetchThenIncrement32
   return atomic_add_value(ptr, 0x00000001);
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)+= 1;
   privateUnlock(ptr);

   return tmp;
#endif /* nativeFetchThenIncrement32 */
}

Uint32
SDL_AtomicFetchThenDecrement32(volatile Uint32 * ptr)
{
#ifdef nativeFetchThenDecrement32
   return atomic_sub_value(ptr, 0x00000001);
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr) -= 1;
   privateUnlock(ptr);

   return tmp;
#endif /* nativeFetchThenDecrement32 */
}

Uint32
SDL_AtomicFetchThenAdd32(volatile Uint32 * ptr, Uint32 value)
{
#ifdef nativeFetchThenAdd32
   return atomic_add_value(ptr, value);
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)+= value;
   privateUnlock(ptr);

   return tmp;
#endif /* nativeFetchThenAdd32 */
}

Uint32
SDL_AtomicFetchThenSubtract32(volatile Uint32 * ptr, Uint32 value)
{
#ifdef nativeFetchThenSubtract32
   return atomic_sub_value(ptr, value);
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   tmp = *ptr;
   (*ptr)-= value;
   privateUnlock(ptr);

   return tmp;
#endif /* nativeFetchThenSubtract32 */
}

Uint32
SDL_AtomicIncrementThenFetch32(volatile Uint32 * ptr)
{
#ifdef nativeIncrementThenFetch32
   atomic_add(ptr, 0x00000001);
   return atomic_add_value(ptr, 0x00000000);
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)+= 1;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif /* nativeIncrementThenFetch32 */
}

Uint32
SDL_AtomicDecrementThenFetch32(volatile Uint32 * ptr)
{
#ifdef nativeDecrementThenFetch32
   atomic_sub(ptr, 0x00000001);
   return atomic_sub_value(ptr, 0x00000000);
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)-= 1;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif /* nativeDecrementThenFetch32 */
}

Uint32
SDL_AtomicAddThenFetch32(volatile Uint32 * ptr, Uint32 value)
{
#ifdef nativeAddThenFetch32
   atomic_add(ptr, value);
   return atomic_add_value(ptr, 0x00000000);
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)+= value;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif /* nativeAddThenFetch32 */
}

Uint32
SDL_AtomicSubtractThenFetch32(volatile Uint32 * ptr, Uint32 value)
{
#ifdef nativeSubtractThenFetch32
   atomic_sub(ptr, value);
   return atomic_sub_value(ptr, 0x00000000);
#else
   Uint32 tmp = 0;

   privateWaitLock(ptr);
   (*ptr)-= value;
   tmp = *ptr;
   privateUnlock(ptr);

   return tmp;
#endif /* nativeSubtractThenFetch32 */
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
#endif /* nativeTestThenSet64 */
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
#endif /* nativeClear64 */
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
#endif /* nativeFetchThenIncrement64 */
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
#endif /* nativeFetchThenDecrement64 */
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
#endif /* nativeFetchThenAdd64 */
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
#endif /* nativeFetchThenSubtract64 */
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
#endif /* nativeIncrementThenFetch64 */
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
#endif /* nativeDecrementThenFetch64 */
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
#endif /* nativeAddThenFetch64 */
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
#endif /* nativeSubtractThenFetch64 */
}

#endif /* SDL_HAS_64BIT_TYPE */
