// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Wrapper around Flag that lets callers wait for the flag to change.

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "Common/Flag.h"

namespace Common
{
class WaitableFlag final
{
public:
  void Set(bool value = true)
  {
    if (m_flag.TestAndSet(value))
    {
      // Lock and immediately unlock m_mutex.
      {
        // Holding the lock at any time between the change of our flag and notify call
        // is sufficient to prevent a race where both of these actions
        // happen between the other thread's predicate test and wait call
        // which would cause wait to block until the next spurious wakeup or timeout.

        // Unlocking before notification is a micro-optimization to prevent
        // the notified thread from immediately blocking on the mutex.
        std::lock_guard<std::mutex> lk(m_mutex);
      }

      m_condvar.notify_all();
    }
  }

  void Reset() { Set(false); }

  void Wait(bool expected_value)
  {
    if (m_flag.IsSet() == expected_value)
      return;

    std::unique_lock<std::mutex> lk(m_mutex);
    m_condvar.wait(lk, [&] { return m_flag.IsSet() == expected_value; });
  }

  template <class Rep, class Period>
  bool WaitFor(bool expected_value, const std::chrono::duration<Rep, Period>& rel_time)
  {
    if (m_flag.IsSet() == expected_value)
      return true;

    std::unique_lock<std::mutex> lk(m_mutex);
    bool signaled =
        m_condvar.wait_for(lk, rel_time, [&] { return m_flag.IsSet() == expected_value; });

    return signaled;
  }

private:
  Flag m_flag;
  std::condition_variable m_condvar;
  std::mutex m_mutex;
};

}  // namespace Common
