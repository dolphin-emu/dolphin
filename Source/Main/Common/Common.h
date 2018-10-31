// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined _WIN32

// Memory leak checks
#define CHECK_HEAP_INTEGRITY()

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
#endif

#ifdef _MSC_VER
#define __getcwd _getcwd
#define __chdir _chdir
#else
#define __getcwd getcwd
#define __chdir chdir
#endif

// Dummy macro for marking translatable strings that can not be immediately translated.
#define _trans(a) a
