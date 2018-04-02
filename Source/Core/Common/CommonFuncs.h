// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include "Common/CommonTypes.h"

// Will fail to compile on a non-array:
template <typename T, size_t N>
constexpr size_t ArraySize(T (&arr)[N])
{
  return N;
}

#define b2(x) ((x) | ((x) >> 1))
#define b4(x) (b2(x) | (b2(x) >> 2))
#define b8(x) (b4(x) | (b4(x) >> 4))
#define b16(x) (b8(x) | (b8(x) >> 8))
#define b32(x) (b16(x) | (b16(x) >> 16))
#define ROUND_UP_POW2(x) (b32(x - 1) + 1)

#ifndef _WIN32

// go to debugger mode
#define Crash()                                                                                    \
  {                                                                                                \
    __builtin_trap();                                                                              \
  }

#else  // WIN32
// Function Cross-Compatibility
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define unlink _unlink
#define vscprintf _vscprintf

// 64 bit offsets for Windows
#define fseeko _fseeki64
#define ftello _ftelli64
#define atoll _atoi64
#define stat _stat64
#define fstat _fstat64
#define fileno _fileno

extern "C" {
__declspec(dllimport) void __stdcall DebugBreak(void);
}
#define Crash()                                                                                    \
  {                                                                                                \
    DebugBreak();                                                                                  \
  }
#endif  // WIN32 ndef

// Wrapper function to get last strerror(errno) string.
// This function might change the error code.
std::string LastStrerrorString();

#ifdef _WIN32
// Wrapper function to get GetLastError() string.
// This function might change the error code.
std::string GetLastErrorString();
#endif
