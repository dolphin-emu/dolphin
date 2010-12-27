// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _ATOMIC_GCC_H_
#define _ATOMIC_GCC_H_

#include "Common.h"

// Atomic operations are performed in a single step by the CPU. It is
// impossible for other threads to see the operation "half-done."
//
// Some atomic operations can be combined with different types of memory
// barriers called "Acquire semantics" and "Release semantics", defined below.
//
// Acquire semantics: Future memory accesses cannot be relocated to before the
//                    operation.
//
// Release semantics: Past memory accesses cannot be relocated to after the
//                    operation.
//
// These barriers affect not only the compiler, but also the CPU.

namespace Common
{

inline void AtomicAdd(volatile u32& target, u32 value) {
	__sync_add_and_fetch(&target, value);
}

inline void AtomicAnd(volatile u32& target, u32 value) {
	__sync_and_and_fetch(&target, value);
}

inline void AtomicDecrement(volatile u32& target) {
	__sync_add_and_fetch(&target, -1);
}

inline void AtomicIncrement(volatile u32& target) {
	__sync_add_and_fetch(&target, 1);
}

inline u32 AtomicLoad(volatile u32& src) {
	return src; // 32-bit reads are always atomic.
}
inline u32 AtomicLoadAcquire(volatile u32& src) {
	//keep the compiler from caching any memory references
	u32 result = src; // 32-bit reads are always atomic.
	//__sync_synchronize(); // TODO: May not be necessary.
	// Compiler instruction only. x86 loads always have acquire semantics.
	__asm__ __volatile__ ( "":::"memory" );
	return result;
}

inline void AtomicOr(volatile u32& target, u32 value) {
	__sync_or_and_fetch(&target, value);
}

inline void AtomicStore(volatile u32& dest, u32 value) {
	dest = value; // 32-bit writes are always atomic.
}
inline void AtomicStoreRelease(volatile u32& dest, u32 value) {
	__sync_lock_test_and_set(&dest, value); // TODO: Wrong! This function is has acquire semantics.
}

}

// Old code kept here for reference in case we need the parts with __asm__ __volatile__.
#if 0
LONG SyncInterlockedIncrement(LONG *Dest)
{
#if defined(__GNUC__) && defined (__GNUC_MINOR__) && ((4 < __GNUC__) || (4 == __GNUC__ && 1 <= __GNUC_MINOR__))
  return  __sync_add_and_fetch(Dest, 1);
#else
  register int result;
  __asm__ __volatile__("lock; xadd %0,%1"
                       : "=r" (result), "=m" (*Dest)
                       : "0" (1), "m" (*Dest)
                       : "memory");
  return result;
#endif
}

LONG SyncInterlockedExchangeAdd(LONG *Dest, LONG Val)
{
#if defined(__GNUC__) && defined (__GNUC_MINOR__) && ((4 < __GNUC__) || (4 == __GNUC__ && 1 <= __GNUC_MINOR__))
  return  __sync_add_and_fetch(Dest, Val);
#else
  register int result;
  __asm__ __volatile__("lock; xadd %0,%1"
                       : "=r" (result), "=m" (*Dest)
                       : "0" (Val), "m" (*Dest)
                       : "memory");
  return result;
#endif
}

LONG SyncInterlockedExchange(LONG *Dest, LONG Val)
{
#if defined(__GNUC__) && defined (__GNUC_MINOR__) && ((4 < __GNUC__) || (4 == __GNUC__ && 1 <= __GNUC_MINOR__))
  return  __sync_lock_test_and_set(Dest, Val);
#else
  register int result;
  __asm__ __volatile__("lock; xchg %0,%1"
                       : "=r" (result), "=m" (*Dest)
                       : "0" (Val), "m" (*Dest)
                       : "memory");
  return result;
#endif
}
#endif

#endif
