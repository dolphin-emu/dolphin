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

#ifndef _ATOMIC_WIN32_H_
#define _ATOMIC_WIN32_H_

#include "Common.h"
#include <intrin.h>
#include <Windows.h>

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

inline void AtomicAdd(volatile u32& target, u32 value) {
	InterlockedExchangeAdd((volatile LONG*)&target, (LONG)value);
}

inline void AtomicAnd(volatile u32& target, u32 value) {
	_InterlockedAnd((volatile LONG*)&target, (LONG)value);
}

inline void AtomicIncrement(volatile u32& target) {
	InterlockedIncrement((volatile LONG*)&target);
}

inline void AtomicDecrement(volatile u32& target) {
	InterlockedDecrement((volatile LONG*)&target);
}

inline u32 AtomicLoad(volatile u32& src) {
	return src; // 32-bit reads are always atomic.
}
inline u32 AtomicLoadAcquire(volatile u32& src) {
	u32 result = src; // 32-bit reads are always atomic.
	_ReadBarrier(); // Compiler instruction only. x86 loads always have acquire semantics.
	return result;
}

inline void AtomicOr(volatile u32& target, u32 value) {
	_InterlockedOr((volatile LONG*)&target, (LONG)value);
}

inline void AtomicStore(volatile u32& dest, u32 value) {
	dest = value; // 32-bit writes are always atomic.
}
inline void AtomicStoreRelease(volatile u32& dest, u32 value) {
	_WriteBarrier(); // Compiler instruction only. x86 stores always have release semantics.
	dest = value; // 32-bit writes are always atomic.
}

}

#endif
