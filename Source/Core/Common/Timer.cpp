// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Timer.h"

#include <chrono>
#include <string>

#ifdef _WIN32
#include <cwchar>

#include <windows.h>
#include <mmsystem.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

namespace Common
{
u32 Timer::GetTimeMs()
{
#ifdef _WIN32
  return timeGetTime();
#elif defined __APPLE__
  struct timeval t;
  (void)gettimeofday(&t, nullptr);
  return ((u32)(t.tv_sec * 1000 + t.tv_usec / 1000));
#else
  struct timespec t;
  (void)clock_gettime(CLOCK_MONOTONIC, &t);
  return ((u32)(t.tv_sec * 1000 + t.tv_nsec / 1000000));
#endif
}

#ifdef _WIN32
double GetFreq()
{
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  return 1000000.0 / double(freq.QuadPart);
}
#endif

u64 Timer::GetTimeUs()
{
#ifdef _WIN32
  LARGE_INTEGER time;
  static double freq = GetFreq();
  QueryPerformanceCounter(&time);
  return u64(double(time.QuadPart) * freq);
#elif defined __APPLE__
  struct timeval t;
  (void)gettimeofday(&t, nullptr);
  return ((u64)(t.tv_sec * 1000000 + t.tv_usec));
#else
  struct timespec t;
  (void)clock_gettime(CLOCK_MONOTONIC, &t);
  return ((u64)(t.tv_sec * 1000000 + t.tv_nsec / 1000));
#endif
}

// --------------------------------------------
// Initiate, Start, Stop, and Update the time
// --------------------------------------------

// Set initial values for the class
Timer::Timer() : m_LastTime(0), m_StartTime(0), m_Running(false)
{
  Update();
}

// Write the starting time
void Timer::Start()
{
  m_StartTime = GetTimeMs();
  m_Running = true;
}

// Stop the timer
void Timer::Stop()
{
  // Write the final time
  m_LastTime = GetTimeMs();
  m_Running = false;
}

// Update the last time variable
void Timer::Update()
{
  m_LastTime = GetTimeMs();
  // TODO(ector) - QPF
}

// -------------------------------------
// Get time difference and elapsed time
// -------------------------------------

// Get the number of milliseconds since the last Update()
u64 Timer::GetTimeDifference()
{
  return GetTimeMs() - m_LastTime;
}

// Add the time difference since the last Update() to the starting time.
// This is used to compensate for a paused game.
void Timer::AddTimeDifference()
{
  m_StartTime += GetTimeDifference();
}

// Get the time elapsed since the Start()
u64 Timer::GetTimeElapsed()
{
  // If we have not started yet, return zero
  if (m_StartTime == 0)
    return 0;

  // Return the final timer time if the timer is stopped
  if (!m_Running)
    return (m_LastTime - m_StartTime);

  return (GetTimeMs() - m_StartTime);
}

// Get the formatted time elapsed since the Start()
std::string Timer::GetTimeElapsedFormatted() const
{
  // If we have not started yet, return zero
  if (m_StartTime == 0)
    return "00:00:00:000";

  // The number of milliseconds since the start.
  // Use a different value if the timer is stopped.
  u64 Milliseconds;
  if (m_Running)
    Milliseconds = GetTimeMs() - m_StartTime;
  else
    Milliseconds = m_LastTime - m_StartTime;
  // Seconds
  u32 Seconds = (u32)(Milliseconds / 1000);
  // Minutes
  u32 Minutes = Seconds / 60;
  // Hours
  u32 Hours = Minutes / 60;

  return fmt::format("{:02}:{:02}:{:02}:{:03}", Hours, Minutes % 60, Seconds % 60,
                     Milliseconds % 1000);
}

// Get current time
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

// Get the number of seconds since January 1 1970
u64 Timer::GetTimeSinceJan1970()
{
  time_t ltime;
  time(&ltime);
  return ((u64)ltime);
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

// Return the current time formatted as Minutes:Seconds:Milliseconds
// in the form 00:00:000.
std::string Timer::GetTimeFormatted()
{
  time_t sysTime;
  time(&sysTime);

  struct tm* gmTime = localtime(&sysTime);

#ifdef _WIN32
  wchar_t tmp[13];
  wcsftime(tmp, 6, L"%M:%S", gmTime);
#else
  char tmp[13];
  strftime(tmp, 6, "%M:%S", gmTime);
#endif

// Now tack on the milliseconds
#ifdef _WIN32
  struct timeb tp;
  (void)::ftime(&tp);
  return WStringToUTF8(tmp) + fmt::format(":{:03}", tp.millitm);
#elif defined __APPLE__
  struct timeval t;
  (void)gettimeofday(&t, nullptr);
  return fmt::format("{}:{:03}", tmp, t.tv_usec / 1000);
#else
  struct timespec t;
  (void)clock_gettime(CLOCK_MONOTONIC, &t);
  return fmt::format("{}:{:03}", tmp, t.tv_nsec / 1000000);
#endif
}

// Returns a timestamp with decimals for precise time comparisons
double Timer::GetDoubleTime()
{
  // FYI: std::chrono::system_clock epoch is not required to be 1970 until c++20.
  // We will however assume time_t IS unix time.
  using Clock = std::chrono::system_clock;

  // TODO: Use this on switch to c++20:
  // const auto since_epoch = Clock::now().time_since_epoch();
  const auto unix_epoch = Clock::from_time_t({});
  const auto since_epoch = Clock::now() - unix_epoch;

  const auto since_double_time_epoch = since_epoch - std::chrono::seconds(DOUBLE_TIME_OFFSET);
  return std::chrono::duration_cast<std::chrono::duration<double>>(since_double_time_epoch).count();
}

// Formats a timestamp from GetDoubleTime() into a date and time string
std::string Timer::GetDateTimeFormatted(double time)
{
  // revert adjustments from GetDoubleTime() to get a normal Unix timestamp again
  time_t seconds = (time_t)time + DOUBLE_TIME_OFFSET;
  tm* localTime = localtime(&seconds);

#ifdef _WIN32
  wchar_t tmp[32] = {};
  wcsftime(tmp, sizeof(tmp), L"%x %X", localTime);
  return WStringToUTF8(tmp);
#else
  char tmp[32] = {};
  strftime(tmp, sizeof(tmp), "%x %X", localTime);
  return tmp;
#endif
}

}  // Namespace Common
