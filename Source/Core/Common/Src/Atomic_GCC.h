// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

inline void AtomicOr(volatile u32& target, u32 value) {
	__sync_or_and_fetch(&target, value);
}

#ifdef __clang__
template <typename T>
_Atomic(T)* ToC11Atomic(volatile T* loc)
{
	return (_Atomic(T)*) loc;
}

#define __atomic_load_n(p, m) __c11_atomic_load(ToC11Atomic(p), m)
#define __atomic_store_n(p, v, m) __c11_atomic_store(ToC11Atomic(p), v, m)
#define __atomic_exchange_n(p, v, m) __c11_atomic_exchange(ToC11Atomic(p), v, m)
#endif

template <typename T>
inline T AtomicLoad(volatile T& src) {
	return __atomic_load_n(&src, __ATOMIC_RELAXED);
}

template <typename T>
inline T AtomicLoadAcquire(volatile T& src) {
	return __atomic_load_n(&src, __ATOMIC_ACQUIRE);
}

template <typename T, typename U>
inline void AtomicStore(volatile T& dest, U value) {
	__atomic_store_n(&dest, value, __ATOMIC_RELAXED);
}

template <typename T, typename U>
inline void AtomicStoreRelease(volatile T& dest, U value) {
	__atomic_store_n(&dest, value, __ATOMIC_RELEASE);
}

template <typename T, typename U>
inline T* AtomicExchangeAcquire(T* volatile& loc, U newval) {
	return __atomic_exchange_n(&loc, newval, __ATOMIC_ACQ_REL);
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
