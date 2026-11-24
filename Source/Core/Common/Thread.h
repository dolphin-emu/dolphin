// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <condition_variable>
#include <stop_token>
#include <thread>

#ifndef _WIN32
#include <tuple>
#endif

// Don't include Common.h here as it will break LogManager
#include "Common/CommonTypes.h"
#include "Common/Mutex.h"

namespace Common
{
int CurrentThreadId();

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask);
void SetCurrentThreadAffinity(u32 mask);

void SleepCurrentThread(int ms);

// Use this function during a spin-wait to make the current thread
// relax while another thread is working. This may be more efficient
// than using events because event functions use kernel calls.
inline void YieldCPU()
{
  std::this_thread::yield();
}

void SetCurrentThreadName(const char* name);

#ifndef _WIN32
// Returns the lowest address of the stack and the size of the stack
std::tuple<void*, size_t> GetCurrentThreadStack();
#endif

// Returns true if a stop is requested before sleep completes.
template <typename Clock, typename Duration>
bool InterruptableSleepUntil(std::stop_token stoken,
                             std::chrono::time_point<Clock, Duration> sleep_time)
{
  DummyMutex dummy_mutex;
  return std::condition_variable_any{}.wait_until(dummy_mutex, stoken, sleep_time,
                                                  [&] { return stoken.stop_requested(); });
}

// Returns true if a stop is requested before sleep completes.
template <typename Rep, typename Period>
bool InterruptableSleepFor(std::stop_token stoken, std::chrono::duration<Rep, Period> sleep_time)
{
  DummyMutex dummy_mutex;
  return std::condition_variable_any{}.wait_for(dummy_mutex, stoken, sleep_time,
                                                [&] { return stoken.stop_requested(); });
}

}  // namespace Common
