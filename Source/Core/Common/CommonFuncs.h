// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include "Common/CommonTypes.h"

// Will fail to compile on a non-array:
#if defined(_MSC_VER) && _MSC_VER <= 1800
// TODO: make this a function when constexpr is available
template <typename T>
struct ArraySizeImpl : public std::extent<T>
{
  static_assert(std::is_array<T>::value, "is array");
};

#define ArraySize(x) ArraySizeImpl<decltype(x)>::value
#else
template <typename T, size_t N>
constexpr size_t ArraySize(T (&arr)[N])
{
  return N;
}
#endif

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

// GCC 4.8 defines all the rotate functions now
// Small issue with GCC's lrotl/lrotr intrinsics is they are still 32bit while we require 64bit
#ifndef _rotl
inline u32 _rotl(u32 x, int shift)
{
  shift &= 31;
  if (!shift)
    return x;
  return (x << shift) | (x >> (32 - shift));
}

inline u32 _rotr(u32 x, int shift)
{
  shift &= 31;
  if (!shift)
    return x;
  return (x >> shift) | (x << (32 - shift));
}
#endif

inline u64 _rotl64(u64 x, unsigned int shift)
{
  unsigned int n = shift % 64;
  return (x << n) | (x >> (64 - n));
}

inline u64 _rotr64(u64 x, unsigned int shift)
{
  unsigned int n = shift % 64;
  return (x >> n) | (x << (64 - n));
}

#else  // WIN32
// Function Cross-Compatibility
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define unlink _unlink
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define snprintf _snprintf
#endif
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

#if (_MSC_VER > 1800)
#else
#define alignof(x) __alignof(x)
#endif
#endif  // WIN32 ndef

// Wrapper function to get last strerror(errno) string.
// This function might change the error code.
std::string LastStrerrorString();

#ifdef _WIN32
// Wrapper function to get GetLastError() string.
// This function might change the error code.
std::string GetLastErrorString();
#endif
