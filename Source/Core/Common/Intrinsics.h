// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(__GNUC__) || defined(__clang__)
// Due to limitations in GCC, intrinsics are only available when compiling with the corresponding
// instruction set enabled. However, using the target attribute, we can compile single functions
// with a different target instruction set, while still creating a generic build.
//
// Since this instruction set is enabled per-function, any callers should verify that the
// instruction set is supported at runtime before calling it, and provide a fallback implementation
// when not supported.
//
// When building with -march=native, or enabling the instruction sets in the compile flags, permit
// usage of the instrinsics without any function attributes. If the command-line architecture does
// not support this instruction set, enable it via function targeting.

#if defined(_M_X86)
#include <x86intrin.h>
#ifndef __SSE4_2__  // (-msse4.2) Streaming SIMD Extensions 4.2
#define TARGET_X86_SSE42 [[gnu::target("sse4.2")]]
#endif
#ifndef __SSE4_1__  // (-msse4.1) Streaming SIMD Extensions 4.1
#define TARGET_X86_SSR41 [[gnu::target("sse4.1")]]
#endif
#ifndef __SSSE3__  // (-mssse3) Supplemental Streaming SIMD Extensions 3
#define TARGET_X86_SSSE3 [[gnu::target("ssse3")]]
#endif
#ifndef __SSE3__  // (-msse3) Streaming SIMD Extensions 3
#define TARGET_X86_SSE3 [[gnu::target("sse3")]]
#endif
#ifndef __SHA__  // (-msha) Intel SHA Extensions
#define TARGET_X86_SHA [[gnu::target("sha")]]
#endif
#ifndef __AES__  // (-maes) Advanced Encryption Standard Instruction Set
#define TARGET_X86_AES [[gnu::target("aes")]]
#endif

#elif defined(_M_ARM_64)
#include <arm_acle.h>
#include <arm_neon.h>
// This would be where some TARGET_ARMV8 macros would go... if only Clang didn't have broken support
// for AArch64 target function attributes. [[gnu::target("arch=armv8-a+crypto")]] works on GCC, but
// not Clang. As well, [[gnu::target("crypto")]] also fails to do anything on Clang, and would need
// to be [[gnu::target("+crypto")]] for GCC (annoying). To work around this, use -march targetting
// for a source file in CMakeLists.txt when needed. This runs the risk of cross-pollination, but it
// seems to be the only way.
#endif

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
// MSVC and ICC support intrinsics for any instruction set without any function attributes.
#include <intrin.h>

#endif

// Define the TARGET macros as nothing if they are not needed, or are unavailable on a given
// platform. This way, a function compiled on multiple architectures can still target, say, an
// x86 extension, so long as the relevant code is contained within an #ifdef _M_X86 guard.

#ifndef TARGET_X86_SSE42
#define TARGET_X86_SSE42
#endif
#ifndef TARGET_X86_SSR41
#define TARGET_X86_SSR41
#endif
#ifndef TARGET_X86_SSSE3
#define TARGET_X86_SSSE3
#endif
#ifndef TARGET_X86_SSE3
#define TARGET_X86_SSE3
#endif
#ifndef TARGET_X86_AES
#define TARGET_X86_AES
#endif
#ifndef TARGET_X86_SHA
#define TARGET_X86_SHA
#endif

#ifndef TARGET_ARMV8_SHA1
#define TARGET_ARMV8_SHA1
#endif
