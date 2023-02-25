// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if !defined(_WIN32)
#include "Common/PerformanceCounter.h"

#include <cstdint>
#include <ctime>

#include <unistd.h>

#include "Common/CommonTypes.h"

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
