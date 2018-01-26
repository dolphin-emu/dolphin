// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Force enable logging in the right modes. For some reason, something had changed
// so that debugfast no longer logged.
#if defined(_DEBUG) || defined(DEBUGFAST)
#undef LOGGING
#define LOGGING 1
#endif

#if defined(__GNUC__) || __clang__
// Disable "unused function" warnings for the ones manually marked as such.
#define UNUSED __attribute__((unused))
#else
// Not sure MSVC even checks this...
#define UNUSED
#endif

#if defined _WIN32

// Memory leak checks
#define CHECK_HEAP_INTEGRITY()

// Alignment
#define GC_ALIGNED16(x) __declspec(align(16)) x
#define GC_ALIGNED32(x) __declspec(align(32)) x
#define GC_ALIGNED64(x) __declspec(align(64)) x
#define GC_ALIGNED128(x) __declspec(align(128)) x
#define GC_ALIGNED16_DECL(x) __declspec(align(16)) x
#define GC_ALIGNED64_DECL(x) __declspec(align(64)) x

// Debug definitions
#if defined(_DEBUG)
#include <crtdbg.h>
#undef CHECK_HEAP_INTEGRITY
#define CHECK_HEAP_INTEGRITY()                                                                     \
  {                                                                                                \
    if (!_CrtCheckMemory())                                                                        \
      PanicAlert("memory corruption detected. see log.");                                          \
  }
// If you want to see how much a pain in the ass singletons are, for example:
// {614} normal block at 0x030C5310, 188 bytes long.
// Data: <Master Log      > 4D 61 73 74 65 72 20 4C 6F 67 00 00 00 00 00 00
struct CrtDebugBreak
{
  CrtDebugBreak(int spot) { _CrtSetBreakAlloc(spot); }
};
// CrtDebugBreak breakAt(614);
#endif  // end DEBUG/FAST

#endif

// Windows compatibility
#ifndef _WIN32
#include <limits.h>
#define MAX_PATH PATH_MAX

#define __forceinline inline __attribute__((always_inline))
#define GC_ALIGNED16(x) __attribute__((aligned(16))) x
#define GC_ALIGNED32(x) __attribute__((aligned(32))) x
#define GC_ALIGNED64(x) __attribute__((aligned(64))) x
#define GC_ALIGNED128(x) __attribute__((aligned(128))) x
#define GC_ALIGNED16_DECL(x) __attribute__((aligned(16))) x
#define GC_ALIGNED64_DECL(x) __attribute__((aligned(64))) x
#endif

#ifdef _MSC_VER
#define __getcwd _getcwd
#define __chdir _chdir
#else
#define __getcwd getcwd
#define __chdir chdir
#endif

// Dummy macro for marking translatable strings that can not be immediately translated.
// wxWidgets does not have a true dummy macro for this.
#define _trans(a) a

// Host communication.
enum HOST_COMM
{
  // Begin at 10 in case there is already messages with wParam = 0, 1, 2 and so on
  WM_USER_STOP = 10,
  WM_USER_CREATE,
  WM_USER_SETCURSOR,
  WM_USER_JOB_DISPATCH,
};
