// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <semaphore>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Functional.h"
#include "Common/Thread.h"

// This class allows flushing data writes in a delayed manner.
// When SetDirty is called the provided function will be invoked on thread with configured delay.
// Multiple SetDirty calls may produce just one flush, delay based on the last call.

namespace Common
{

class FlushThread final
{
public:
  using FunctionType = Common::MoveOnlyFunction<void()>;

  FlushThread() = default;
  explicit FlushThread(std::string name, FunctionType func)
  {
    Reset(std::move(name), std::move(func));
  }

  ~FlushThread() { Shutdown(); }

  FlushThread(const FlushThread&) = delete;
  FlushThread& operator=(const FlushThread&) = delete;

  FlushThread(FlushThread&&) = delete;
  FlushThread& operator=(FlushThread&&) = delete;

  // May not take effect until clean.
  void SetFlushDelay(DT delay) { m_flush_delay.store(delay, std::memory_order_relaxed); }

  // (Re)Starts the thread with the provided flush function.
  // Other state is unchanged.
  void Reset(std::string name, FunctionType func)
  {
    Shutdown();

    m_want_shutdown.store(false, std::memory_order_relaxed);
    m_thread = std::thread{std::bind_front(&FlushThread::ThreadFunc, this), std::move(name),
                           std::move(func)};
  }

  // Graceful immediate shutdown. Waits for final flush if necessary.
  // Does nothing if thread isn't running.
  void Shutdown()
  {
    if (!m_thread.joinable())
      return;

    WaitForCompletion();
    m_want_shutdown.store(true, std::memory_order_relaxed);
    m_event.Set();
    m_thread.join();
  }

  void SetDirty()
  {
    m_flush_deadline.store((Clock::now() + m_flush_delay.load(std::memory_order_relaxed)));
    m_dirty_count.fetch_add(1, std::memory_order_release);
    m_event.Set();
  }

  // Lets the worker immediately flush if necessary.
  // Does nothing if thread isn't running.
  void WaitForCompletion()
  {
    if (!m_thread.joinable())
      return;

    m_run_freely.release();

    // Wait for m_dirty_count == 0.
    while (auto old_count = m_dirty_count.load(std::memory_order_acquire))
      m_dirty_count.wait(old_count, std::memory_order_acquire);

    m_run_freely.acquire();
  }

private:
  auto GetDeadline() const { return m_flush_deadline.load(std::memory_order_relaxed); }

  void WaitUntilFlushIsWanted()
  {
    while (!m_run_freely.try_acquire_until(GetDeadline()))
    {
      if (Clock::now() >= GetDeadline())
        return;
    }
    m_run_freely.release();
  }

  void ThreadFunc(const std::string& name, const FunctionType& flush_func)
  {
    Common::SetCurrentThreadName(name.c_str());

    while (true)
    {
      m_event.Wait();

      if (m_want_shutdown.load(std::memory_order_relaxed))
        break;

      WaitUntilFlushIsWanted();

      const auto cleaning_count = m_dirty_count.load(std::memory_order_acquire);
      if (cleaning_count != 0)
      {
        flush_func();
        m_dirty_count.fetch_sub(cleaning_count, std::memory_order_release);
        m_dirty_count.notify_all();
      }
    }
  }

  // Incremented when a flush needs to happen.
  // Decremented by worker-thread to signal completion.
  std::atomic<u32> m_dirty_count{};

  std::atomic<DT> m_flush_delay{};
  std::atomic<TimePoint> m_flush_deadline{};

  // Worker tries to acquire this for the flush delay.
  // Releasing it lets the worker run without waiting.
  std::counting_semaphore<> m_run_freely{0};

  std::thread m_thread;
  Common::Event m_event;

  std::atomic_bool m_want_shutdown{};
};

}  // namespace Common
