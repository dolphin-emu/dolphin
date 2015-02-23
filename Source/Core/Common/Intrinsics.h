// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#ifdef _M_X86

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#ifndef __GNUC_PREREQ
#define __GNUC_PREREQ(maj, min) 0
#endif

#if defined __GNUC__ && !__GNUC_PREREQ(4, 9) && !defined __SSSE3__
// GCC <= 4.8 only enables intrinsics based on the command-line.
// GCC >= 4.9 always declares all intrinsics.
// We only want to require SSE2 and thus compile with -msse2,
// so define the one post-SSE2 intrinsic that we dispatch at runtime.
static __inline __m128i __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_mm_shuffle_epi8(__m128i a, __m128i mask)
{
	__m128i result;
	__asm__("pshufb %1, %0"
		: "=x" (result)
		: "xm" (mask), "0" (a));
	return result;
}
#endif

#if defined _M_GENERIC
#  define _M_SSE 0
#elif _MSC_VER || __INTEL_COMPILER
#  define _M_SSE 0x402
#elif defined __GNUC__
# if defined __SSE4_2__
#  define _M_SSE 0x402
# elif defined __SSE4_1__
#  define _M_SSE 0x401
# elif defined __SSSE3__
#  define _M_SSE 0x301
# elif defined __SSE3__
#  define _M_SSE 0x300
# endif
#endif

#endif // _M_X86
