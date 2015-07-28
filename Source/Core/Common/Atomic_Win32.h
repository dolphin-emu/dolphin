// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// IWYU pragma: private, include "Common/Atomic.h"

#pragma once

#include <Windows.h>

#include "Common/Common.h"
#include "Common/Intrinsics.h"

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
//
// NOTE: Acquire and Release are not differentiated right now. They perform a
// full memory barrier instead of a "one-way" memory barrier. The newest
// Windows SDK has Acquire and Release versions of some Interlocked* functions.

namespace Common
{

inline void AtomicAdd(volatile u32& target, u32 value)
{
	_InterlockedExchangeAdd((volatile LONG*)&target, (LONG)value);
}

inline void AtomicAnd(volatile u32& target, u32 value)
{
	_InterlockedAnd((volatile LONG*)&target, (LONG)value);
}

inline void AtomicIncrement(volatile u32& target)
{
	_InterlockedIncrement((volatile LONG*)&target);
}

inline void AtomicDecrement(volatile u32& target)
{
	_InterlockedDecrement((volatile LONG*)&target);
}

inline void AtomicOr(volatile u32& target, u32 value)
{
	_InterlockedOr((volatile LONG*)&target, (LONG)value);
}

template <typename T>
inline T AtomicLoad(volatile T& src)
{
	return src; // 32-bit reads are always atomic.
}

template <typename T>
inline T AtomicLoadAcquire(volatile T& src)
{
	T result = src; // 32-bit reads are always atomic.
	_ReadBarrier(); // Compiler instruction only. x86 loads always have acquire semantics.
	return result;
}

template <typename T, typename U>
inline void AtomicStore(volatile T& dest, U value)
{
	dest = (T) value; // 32-bit writes are always atomic.
}

template <typename T, typename U>
inline void AtomicStoreRelease(volatile T& dest, U value)
{
	_WriteBarrier(); // Compiler instruction only. x86 stores always have release semantics.
	dest = (T) value; // 32-bit writes are always atomic.
}

template <typename T, typename U>
inline T* AtomicExchangeAcquire(T* volatile& loc, U newval)
{
	return (T*) _InterlockedExchangePointer_acq((void* volatile*) &loc, (void*) newval);
}

}
