// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(_M_X86)

/**
 * It is assumed that all compilers used to build Dolphin support intrinsics up to and including
 * SSE 4.2 on x86/x64.
 */

#if defined(__GNUC__) || defined(__clang__)

/**
 * Due to limitations in GCC, SSE intrinsics are only available when compiling with the
 * corresponding instruction set enabled. However, using the target attribute, we can compile
 * single functions with a different target instruction set, while still creating a generic build.
 *
 * Since this instruction set is enabled per-function, any callers should verify that the
 * instruction set is supported at runtime before calling it, and provide a fallback implementation
 * when not supported.
 *
 * When building with -march=native, or enabling the instruction sets in the compile flags, permit
 * usage of the instrinsics without any function attributes. If the command-line architecture does
 * not support this instruction set, enable it via function targeting.
 */

#include <x86intrin.h>
#ifndef __SSE4_2__
#define FUNCTION_TARGET_SSE42 [[gnu::target("sse4.2")]]
#endif
#ifndef __SSE4_1__
#define FUNCTION_TARGET_SSR41 [[gnu::target("sse4.1")]]
#endif
#ifndef __SSSE3__
#define FUNCTION_TARGET_SSSE3 [[gnu::target("ssse3")]]
#endif
#ifndef __SSE3__
#define FUNCTION_TARGET_SSE3 [[gnu::target("sse3")]]
#endif

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

/**
 * MSVC and ICC support intrinsics for any instruction set without any function attributes.
 */
#include <intrin.h>

#endif  // defined(_MSC_VER) || defined(__INTEL_COMPILER)

#endif  // _M_X86

/**
 * Define the FUNCTION_TARGET macros to nothing if they are not needed, or not on an X86 platform.
 * This way when a function is defined with FUNCTION_TARGET you don't need to define a second
 * version without the macro around a #ifdef guard. Be careful when using intrinsics, as all use
 * should still be placed around a #ifdef _M_X86 if the file is compiled on all architectures.
 */
#ifndef FUNCTION_TARGET_SSE42
#define FUNCTION_TARGET_SSE42
#endif
#ifndef FUNCTION_TARGET_SSR41
#define FUNCTION_TARGET_SSR41
#endif
#ifndef FUNCTION_TARGET_SSSE3
#define FUNCTION_TARGET_SSSE3
#endif
#ifndef FUNCTION_TARGET_SSE3
#define FUNCTION_TARGET_SSE3
#endif
