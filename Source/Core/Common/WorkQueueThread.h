// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "Common/Event.h"
#include "Common/SPSCQueue.h"
#include "Common/Thread.h"

// A thread that executes the given function for every item placed into its queue.

namespace Common
{
namespace detail
{
template <typename T, bool IsSingleProducer>
class WorkQueueThreadBase final
{
public:
  WorkQueueThreadBase() = default;
  WorkQueueThreadBase(std::string name, std::function<void(T)> function)
  {
    Reset(std::move(name), std::move(function));
  }
  ~WorkQueueThreadBase() { Shutdown(); }

  // Shuts the current work thread down (if any) and starts a new thread with the given function
  // Note: Some consumers of this API push items to the queue before starting the thread.
  void Reset(std::string name, std::function<void(T)> function)
  {
    auto lg = GetLockGuard();
    Shutdown();
    m_run_thread.store(true, std::memory_order_relaxed);
    m_thread = std::thread(std::bind_front(&WorkQueueThreadBase::ThreadLoop, this), std::move(name),
                           std::move(function));
  }

  // Adds an item to the work queue
  template <typename... Args>
  void EmplaceItem(Args&&... args)
  {
    auto lg = GetLockGuard();
    m_items.Emplace(std::forward<Args>(args)...);
    m_event.Set();
  }
  void Push(T&& item) { EmplaceItem(std::move(item)); }
  void Push(const T& item) { EmplaceItem(item); }

  // Empties the queue, skipping all work.
  // Blocks until the current work is cancelled.
  void Cancel()
  {
    auto lg = GetLockGuard();
    if (IsRunning())
    {
      m_skip_work.store(true, std::memory_order_relaxed);
      WaitForCompletion();
      m_skip_work.store(false, std::memory_order_relaxed);
    }
    else
    {
      m_items.Clear();
    }
  }

  // Tells the worker thread to stop when its queue is empty.
  // Blocks until the worker thread exits. Does nothing if thread isn't running.
  void Shutdown() { StopThread(true); }

  // Tells the worker thread to stop immediately, potentially leaving work in the queue.
  // Blocks until the worker thread exits. Does nothing if thread isn't running.
  void Stop() { StopThread(false); }

  // Stops the worker thread ASAP and empties the queue.
  void StopAndCancel()
  {
    auto lg = GetLockGuard();
    Stop();
    Cancel();
  }

  // Blocks until all items in the queue have been processed (or cancelled)
  // Does nothing if thread isn't running.
  void WaitForCompletion()
  {
    auto lg = GetLockGuard();
    if (IsRunning())
      m_items.WaitForEmpty();
  }

private:
  void StopThread(bool wait_for_completion)
  {
    auto lg = GetLockGuard();

    if (wait_for_completion)
      WaitForCompletion();

    if (m_run_thread.exchange(false, std::memory_order_relaxed))
    {
      m_event.Set();
      m_thread.join();
    }
  }

  auto GetLockGuard()
  {
    struct DummyLockGuard
    {
      // Silences unused variable warning.
      ~DummyLockGuard() { void(); }
    };

    if constexpr (IsSingleProducer)
      return DummyLockGuard{};
    else
      return std::lock_guard{m_mutex};
  }

  bool IsRunning() { return m_thread.joinable(); }

  void ThreadLoop(const std::string& thread_name, const std::function<void(T)>& function)
  {
    Common::SetCurrentThreadName(thread_name.c_str());

    while (m_run_thread.load(std::memory_order_relaxed))
    {
      if (m_items.Empty())
      {
        m_event.Wait();
        continue;
      }

      if (m_skip_work.load(std::memory_order_relaxed))
      {
        m_items.Clear();
        continue;
      }

      function(std::move(m_items.Front()));
      m_items.Pop();
    }
  }

  std::thread m_thread;
  Common::WaitableSPSCQueue<T> m_items;
  Common::Event m_event;
  std::atomic_bool m_skip_work = false;
  std::atomic_bool m_run_thread = false;

  using DummyMutex = std::type_identity<void>;
  using ProducerMutex = std::conditional_t<IsSingleProducer, DummyMutex, std::recursive_mutex>;
  ProducerMutex m_mutex;
};
}  // namespace detail

// Multiple threads may use the public interface.
template <typename T>
using WorkQueueThread = detail::WorkQueueThreadBase<T, false>;

// A "Single Producer" WorkQueueThread.
// It uses no mutex but only one thread can safely manipulate the queue.
template <typename T>
using WorkQueueThreadSP = detail::WorkQueueThreadBase<T, true>;

}  // namespace Common
