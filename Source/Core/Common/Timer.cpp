// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Timer.h"

#include <chrono>

#ifdef _WIN32
#include <Windows.h>
#include <ctime>
#include <timeapi.h>
#else
#include <sys/time.h>
#endif

#include "Common/CommonTypes.h"

namespace Common
{
template <typename Clock, typename Duration>
static typename Clock::rep time_now()
{
  return std::chrono::time_point_cast<Duration>(Clock::now()).time_since_epoch().count();
}

template <typename Duration>
static auto steady_time_now()
{
  return time_now<std::chrono::steady_clock, Duration>();
}

u64 Timer::NowUs()
{
  return steady_time_now<std::chrono::microseconds>();
}

u64 Timer::NowMs()
{
  return steady_time_now<std::chrono::milliseconds>();
}

void Timer::Start()
{
  m_start_ms = NowMs();
  m_end_ms = 0;
  m_running = true;
}

void Timer::StartWithOffset(u64 offset)
{
  Start();
  m_start_ms -= offset;
}

void Timer::Stop()
{
  m_end_ms = NowMs();
  m_running = false;
}

u64 Timer::ElapsedMs() const
{
  const u64 end = m_running ? NowMs() : m_end_ms;
  // Can handle up to 1 rollover event (underflow produces correct result)
  // If Start() has never been called, will return 0
  return end - m_start_ms;
}

u64 Timer::GetLocalTimeSinceJan1970()
{
  // TODO Would really, really like to use std::chrono here, but Windows did not support
  // std::chrono::current_zone() until 19H1, and other compilers don't even provide support for
  // timezone-related parts of chrono. Someday!
  // see https://bugs.dolphin-emu.org/issues/13007#note-4
  time_t sysTime, tzDiff, tzDST;
  time(&sysTime);
  tm* gmTime = localtime(&sysTime);

  // Account for DST where needed
  if (gmTime->tm_isdst == 1)
    tzDST = 3600;
  else
    tzDST = 0;

  // Lazy way to get local time in sec
  gmTime = gmtime(&sysTime);
  tzDiff = sysTime - mktime(gmTime);

  return static_cast<u64>(sysTime + tzDiff + tzDST);
}

void Timer::IncreaseResolution()
{
#ifdef _WIN32
  // Disable execution speed and timer resolution throttling process-wide.
  // This mainly will keep Dolphin marked as high performance if it's in the background. The OS
  // should make it high performance if it's in the foreground anyway (or for some specific
  // threads e.g. audio).
  // This is best-effort (i.e. the call may fail on older versions of Windows, where such throttling
  // doesn't exist, anyway), and we don't bother reverting once set.
  // This adjusts behavior on CPUs with "performance" and "efficiency" cores
  PROCESS_POWER_THROTTLING_STATE PowerThrottling{};
  PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  PowerThrottling.ControlMask =
      PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
  PowerThrottling.StateMask = 0;
  SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &PowerThrottling,
                        sizeof(PowerThrottling));

  // Not actually sure how useful this is these days.. :')
  timeBeginPeriod(1);
#endif
}

void Timer::RestoreResolution()
{
#ifdef _WIN32
  timeEndPeriod(1);
#endif
}

}  // Namespace Common
