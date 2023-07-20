// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/PerformanceCounter.h"

#include "Common/CommonTypes.h"

#if !defined(_WIN32)

#include <ctime>

#include <unistd.h>

#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
#if defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK > 0
#define DOLPHIN_CLOCK CLOCK_MONOTONIC
#else
#define DOLPHIN_CLOCK CLOCK_REALTIME
#endif
#endif

bool QueryPerformanceCounter(u64* out)
{
#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
  timespec tp;
  if (clock_gettime(DOLPHIN_CLOCK, &tp))
    return false;
  *out = (u64)tp.tv_nsec + (u64)1000000000 * (u64)tp.tv_sec;
  return true;
#else
  *out = 0;
  return false;
#endif
}

bool QueryPerformanceFrequency(u64* out)
{
#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
  *out = 1000000000;
  return true;
#else
  *out = 1;
  return false;
#endif
}

#endif

static const LARGE_INTEGER s_cached_performance_frequency = []() -> LARGE_INTEGER {
  LARGE_INTEGER out;
  QueryPerformanceFrequency(&out);
  return out;
}();

// Win32: The result of QueryPerformanceFrequency is fixed at system boot, consistent across all
// processors, and cannot fail on systems running Windows XP or later.
// Linux: We imitate QueryPerformanceFrequency by assuming the frequency is either nanoseconds or
// seconds, depending on if POSIX timers are available (this is known at compile-time).
u64 QueryCachedPerformanceFrequency()
{
#if defined(_WIN32)
  return s_cached_performance_frequency.QuadPart;
#else
  return s_cached_performance_frequency;
#endif
}
