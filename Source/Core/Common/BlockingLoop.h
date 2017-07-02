// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "Common/Event.h"
#include "Common/Flag.h"

namespace Common
{
// This class provides a synchronized loop.
// It's a thread-safe way to trigger a new iteration without busy loops.
// It's optimized for high-usage iterations which usually are already running while it's triggered
// often.
// Be careful when using Wait() and Wakeup() at the same time. Wait() may block forever while
// Wakeup() is called regularly.
class BlockingLoop
{
public:
  enum StopMode
  {
    kNonBlock,
    kBlock,
    kBlockAndGiveUp,
  };

  BlockingLoop() { m_stopped.Set(); }
  ~BlockingLoop() { Stop(kBlockAndGiveUp); }
  // Triggers to rerun the payload of the Run() function at least once again.
  // This function will never block and is designed to finish as fast as possible.
  void Wakeup()
  {
    // Already running, so no need for a wakeup.
    // This is the common case, so try to get this as fast as possible.
    if (m_running_state.load() >= STATE_NEED_EXECUTION)
      return;

    // Mark that new data is available. If the old state will rerun the payload
    // itself, we don't have to set the event to interrupt the worker.
    if (m_running_state.exchange(STATE_NEED_EXECUTION) != STATE_SLEEPING)
      return;

    // Else as the worker thread may sleep now, we have to set the event.
    m_new_work_event.Set();
  }

  // Wait for a complete payload run after the last Wakeup() call.
  // If stopped, this returns immediately.
  void Wait()
  {
    // already done
    if (IsDone())
      return;

    // notifying this event will only wake up one thread, so use a mutex here to
    // allow only one waiting thread. And in this way, we get an event free wakeup
    // but for the first thread for free
    std::lock_guard<std::mutex> lk(m_wait_lock);

    // Wait for the worker thread to finish.
    while (!IsDone())
    {
      m_done_event.Wait();
    }

    // As we wanted to wait for the other thread, there is likely no work remaining.
    // So there is no need for a busy loop any more.
    m_may_sleep.Set();
  }

  // Wait for a complete payload run after the last Wakeup() call.
  // This version will call a yield function every 100ms.
  // If stopped, this returns immediately.
  template <class Rep, class Period, typename Functor>
  void WaitYield(const std::chrono::duration<Rep, Period>& rel_time, Functor yield_func)
  {
    // already done
    if (IsDone())
      return;

    // notifying this event will only wake up one thread, so use a mutex here to
    // allow only one waiting thread. And in this way, we get an event free wakeup
    // but for the first thread for free
    std::lock_guard<std::mutex> lk(m_wait_lock);

    // Wait for the worker thread to finish.
    while (!IsDone())
    {
      if (!m_done_event.WaitFor(rel_time))
        yield_func();
    }

    // As we wanted to wait for the other thread, there is likely no work remaining.
    // So there is no need for a busy loop any more.
    m_may_sleep.Set();
  }

  // Half start the worker.
  // So this object is in a running state and Wait() will block until the worker calls Run().
  // This may be called from any thread and is supposed to be called at least once before Wait() is
  // used.
  void Prepare()
  {
    // There is a race condition if the other threads call this function while
    // the loop thread is initializing. Using this lock will ensure a valid state.
    std::lock_guard<std::mutex> lk(m_prepare_lock);

    if (!m_stopped.TestAndClear())
      return;
    m_running_state.store(
        STATE_LAST_EXECUTION);  // so the payload will only be executed once without any Wakeup call
    m_shutdown.Clear();
    m_may_sleep.Set();
  }

  // Main loop of this object.
  // The payload callback is called at least as often as it's needed to match the Wakeup()
  // requirements.
  // The optional timeout parameter is a timeout for how periodically the payload should be called.
  // Use timeout = 0 to run without a timeout at all.
  template <class F>
  void Run(F payload, int64_t timeout = 0)
  {
    // Asserts that Prepare is called at least once before we enter the loop.
    // But a good implementation should call this before already.
    Prepare();

    while (!m_shutdown.IsSet())
    {
      payload();

      switch (m_running_state.load())
      {
      case STATE_NEED_EXECUTION:
        // We won't get notified while we are in the STATE_NEED_EXECUTION state, so maybe Wakeup was
        // called.
        // So we have to assume on finishing the STATE_NEED_EXECUTION state, that there may be some
        // remaining tasks.
        // To process this tasks, we call the payload again within the STATE_LAST_EXECUTION state.
        m_running_state--;
        break;

      case STATE_LAST_EXECUTION:
        // If we're still in the STATE_LAST_EXECUTION state, then Wakeup wasn't called within the
        // last
        // execution of the payload. This means we should be ready now.
        // But bad luck, Wakeup may have been called right now. So break and rerun the payload
        // if the state was touched.
        if (m_running_state-- != STATE_LAST_EXECUTION)
          break;

        // Else we're likely in the STATE_DONE state now, so wakeup the waiting threads right now.
        // However, if we're not in the STATE_DONE state any more, the event should also be
        // triggered so that we'll skip the next waiting call quite fast.
        m_done_event.Set();

      case STATE_DONE:
        // We're done now. So time to check if we want to sleep or if we want to stay in a busy
        // loop.
        if (m_may_sleep.TestAndClear())
        {
          // Try to set the sleeping state.
          if (m_running_state-- != STATE_DONE)
            break;
        }
        else
        {
          // Busy loop.
          break;
        }

      case STATE_SLEEPING:
        // Just relax
        if (timeout > 0)
        {
          m_new_work_event.WaitFor(std::chrono::milliseconds(timeout));
        }
        else
        {
          m_new_work_event.Wait();
        }
        break;
      }
    }

    // Shutdown down, so get a safe state
    m_running_state.store(STATE_DONE);
    m_stopped.Set();

    // Wake up the last Wait calls.
    m_done_event.Set();
  }

  // Quits the main loop.
  // By default, it will wait until the main loop quits.
  // Be careful to not use the blocking way within the payload of the Run() method.
  void Stop(StopMode mode = kBlock)
  {
    if (m_stopped.IsSet())
      return;

    m_shutdown.Set();

    // We have to interrupt the sleeping call to let the worker shut down soon.
    Wakeup();

    switch (mode)
    {
    case kNonBlock:
      break;
    case kBlock:
      Wait();
      break;
    case kBlockAndGiveUp:
      WaitYield(std::chrono::milliseconds(100), [&] {
        // If timed out, assume no one will come along to call Run, so force a break
        m_stopped.Set();
      });
      break;
    }
  }

  bool IsRunning() const { return !m_stopped.IsSet() && !m_shutdown.IsSet(); }
  bool IsDone() const { return m_stopped.IsSet() || m_running_state.load() <= STATE_DONE; }
  // This function should be triggered regularly over time so
  // that we will fall back from the busy loop to sleeping.
  void AllowSleep() { m_may_sleep.Set(); }
private:
  std::mutex m_wait_lock;
  std::mutex m_prepare_lock;

  Flag m_stopped;   // If this is set, Wait() shall not block.
  Flag m_shutdown;  // If this is set, the loop shall end.

  Event m_new_work_event;
  Event m_done_event;

  enum RUNNING_TYPE
  {
    STATE_SLEEPING = 0,
    STATE_DONE = 1,
    STATE_LAST_EXECUTION = 2,
    STATE_NEED_EXECUTION = 3
  };
  std::atomic<int> m_running_state;  // must be of type RUNNING_TYPE

  Flag m_may_sleep;  // If this is set, we fall back from the busy loop to an event based
                     // synchronization.
};
}
