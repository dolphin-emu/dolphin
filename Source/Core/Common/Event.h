// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Multithreaded event class. This allows waiting in a thread for an event to
// be triggered in another thread. While waiting, the CPU will be available for
// other tasks.
// * Set(): triggers the event and wakes up the waiting thread.
// * Wait(): waits for the event to be triggered.
// * Reset(): tries to reset the event before the waiting thread sees it was
//            triggered. Usually a bad idea.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "Common/Flag.h"
#include "Common/WaitableFlag.h"

namespace Common
{
class TimedEvent final
{
public:
  void Set()
  {
    if (m_flag.TestAndSet())
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

      m_condvar.notify_one();
    }
  }

  void Wait()
  {
    if (m_flag.TestAndClear())
      return;

    std::unique_lock<std::mutex> lk(m_mutex);
    m_condvar.wait(lk, [&] { return m_flag.TestAndClear(); });
  }

  template <class Rep, class Period>
  bool WaitFor(const std::chrono::duration<Rep, Period>& rel_time)
  {
    if (m_flag.TestAndClear())
      return true;

    std::unique_lock<std::mutex> lk(m_mutex);
    bool signaled = m_condvar.wait_for(lk, rel_time, [&] { return m_flag.TestAndClear(); });

    return signaled;
  }

  void Reset()
  {
    // no other action required, since wait loops on
    // the predicate and any lingering signal will get
    // cleared on the first iteration
    m_flag.Clear();
  }

private:
  Flag m_flag;
  std::condition_variable m_condvar;
  std::mutex m_mutex;
};

// An auto-resetting WaitableFlag. Only sensible for one waiting thread.
class Event final
{
public:
  void Set() { m_flag.Set(); }

  void Wait()
  {
    m_flag.Wait(true);

    // This might run concurrently with the next Set, clearing m_flag before notification.
    // "Missing" that event later is fine as long as all the data is visible *now*.
    m_flag.Reset();
    // This store-load barrier prevents the Reset-store ordering after pertinent data loads.
    // Without it, we could observe stale data AND miss the next event, i.e. deadlock.
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }

  void Reset() { m_flag.Reset(); }

private:
  WaitableFlag m_flag{};
};

}  // namespace Common
