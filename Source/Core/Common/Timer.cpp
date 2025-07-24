// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Timer.h"

#include <chrono>
#include <thread>
#include "Common/CommonFuncs.h"

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#else
#include <sys/time.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

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
#ifdef _MSC_VER
  std::chrono::zoned_seconds seconds(
      std::chrono::current_zone(),
      std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()));
  return seconds.get_local_time().time_since_epoch().count();
#else
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
#endif
}

// This is requested by Timer::IncreaseResolution on Windows.
// On Linux/other we hope/assume we already have 1ms scheduling granularity.
static constexpr int TIMER_RESOLUTION_MS = 1;

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
  timeBeginPeriod(TIMER_RESOLUTION_MS);
#endif
}

void Timer::RestoreResolution()
{
#ifdef _WIN32
  timeEndPeriod(TIMER_RESOLUTION_MS);
#endif
}

PrecisionTimer::PrecisionTimer()
{
#if defined(_WIN32)
  // "TIMER_HIGH_RESOLUTION" requires Windows 10, version 1803, and later.
  m_timer_handle =
      CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

  if (m_timer_handle == NULL)
  {
    ERROR_LOG_FMT(COMMON, "CREATE_WAITABLE_TIMER_HIGH_RESOLUTION: Error:{}", GetLastError());

    // Create a normal timer if "HIGH_RESOLUTION" isn't available.
    m_timer_handle = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    if (m_timer_handle == NULL)
      ERROR_LOG_FMT(COMMON, "CreateWaitableTimerExW: Error:{}", GetLastError());
  }
#endif
}

PrecisionTimer::~PrecisionTimer()
{
#if defined(_WIN32)
  CloseHandle(m_timer_handle);
#endif
}

void PrecisionTimer::SleepUntil(Clock::time_point target)
{
  constexpr auto SPIN_TIME =
      std::chrono::milliseconds{TIMER_RESOLUTION_MS} + std::chrono::microseconds{20};

#if defined(_WIN32)
  while (true)
  {
    // SetWaitableTimerEx takes time in "100 nanosecond intervals".
    using TimerDuration = std::chrono::duration<LONGLONG, std::ratio<100, std::nano::den>::type>;

    // Apparently waiting longer than the timer resolution gives terrible accuracy.
    // We'll wait in steps of 95% of the time period.
    constexpr auto MAX_TICKS =
        duration_cast<TimerDuration>(std::chrono::milliseconds{TIMER_RESOLUTION_MS}) * 95 / 100;

    const auto wait_time = target - Clock::now() - SPIN_TIME;
    const auto ticks = std::min(duration_cast<TimerDuration>(wait_time), MAX_TICKS).count();
    if (ticks <= 0)
      break;

    const LARGE_INTEGER due_time{.QuadPart = -ticks};
    SetWaitableTimerEx(m_timer_handle, &due_time, 0, NULL, NULL, NULL, 0);
    WaitForSingleObject(m_timer_handle, INFINITE);
  }
#else
  // Sleeping on Linux generally isn't as terrible as it is on Windows.
  std::this_thread::sleep_until(target - SPIN_TIME);
#endif

  // Spin for the remaining time.
  while (Clock::now() < target)
  {
#if defined(_WIN32)
    YieldProcessor();
#else
    std::this_thread::yield();
#endif
  }
}

// Results are appropriately slewed on Linux, but not on Windows, macOS, or FreeBSD.
// Clocks with that functionality seem to not be available there.
auto SteadyAwakeClock::now() -> time_point
{
#if defined(_WIN32)
  // The count is system time "in units of 100 nanoseconds".
  using InterruptDuration = std::chrono::duration<ULONGLONG, std::ratio<100, std::nano::den>::type>;

  ULONGLONG interrupt_time{};
  if (!QueryUnbiasedInterruptTime(&interrupt_time))
    ERROR_LOG_FMT(COMMON, "QueryUnbiasedInterruptTime");

  return time_point{InterruptDuration{interrupt_time}};
#else
  // Note that Linux's CLOCK_MONOTONIC "does not count time that the system is suspended".
  // This is in contrast to the behavior on macOS and FreeBSD.
  static constexpr auto clock_id =
#if defined(__linux__)
      CLOCK_MONOTONIC;
#elif defined(__APPLE__)
      CLOCK_UPTIME_RAW;
#else
      CLOCK_UPTIME;
#endif

  timespec ts{};
  if (clock_gettime(clock_id, &ts) != 0)
    ERROR_LOG_FMT(COMMON, "clock_gettime: {}", LastStrerrorString());

  return time_point{std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec}};
#endif
}

}  // Namespace Common
