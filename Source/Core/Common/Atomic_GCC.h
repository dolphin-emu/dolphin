// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// IWYU pragma: private, include "Common/Atomic.h"

#pragma once

#include "Common/Common.h"
#include "Common/CommonTypes.h"

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
inline void AtomicAdd(volatile u32& target, u32 value)
{
  __sync_add_and_fetch(&target, value);
}

inline void AtomicAnd(volatile u32& target, u32 value)
{
  __sync_and_and_fetch(&target, value);
}

inline void AtomicDecrement(volatile u32& target)
{
  __sync_add_and_fetch(&target, -1);
}

inline void AtomicIncrement(volatile u32& target)
{
  __sync_add_and_fetch(&target, 1);
}

inline void AtomicOr(volatile u32& target, u32 value)
{
  __sync_or_and_fetch(&target, value);
}

#ifndef __ATOMIC_RELAXED
#error __ATOMIC_RELAXED not defined; your compiler version is too old.
#endif

template <typename T>
inline T AtomicLoad(volatile T& src)
{
  return __atomic_load_n(&src, __ATOMIC_RELAXED);
}

template <typename T>
inline T AtomicLoadAcquire(volatile T& src)
{
  return __atomic_load_n(&src, __ATOMIC_ACQUIRE);
}

template <typename T, typename U>
inline void AtomicStore(volatile T& dest, U value)
{
  __atomic_store_n(&dest, value, __ATOMIC_RELAXED);
}

template <typename T, typename U>
inline void AtomicStoreRelease(volatile T& dest, U value)
{
  __atomic_store_n(&dest, value, __ATOMIC_RELEASE);
}

template <typename T, typename U>
inline T* AtomicExchangeAcquire(T* volatile& loc, U newval)
{
  return __atomic_exchange_n(&loc, newval, __ATOMIC_ACQ_REL);
}
}  // namespace Common
