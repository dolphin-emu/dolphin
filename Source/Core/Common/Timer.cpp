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
  if (m_start_ms > offset)
    m_start_ms -= offset;
}

void Timer::Stop()
{
  m_end_ms = NowMs();
  m_running = false;
}

u64 Timer::ElapsedMs() const
{
  // If we have not started yet, return zero
  if (m_start_ms == 0)
    return 0;

  if (m_running)
  {
    u64 now = NowMs();
    if (m_start_ms >= now)
      return 0;
    return now - m_start_ms;
  }
  else
  {
    if (m_start_ms >= m_end_ms)
      return 0;
    return m_end_ms - m_start_ms;
  }
}

u64 Timer::GetLocalTimeSinceJan1970()
{
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
