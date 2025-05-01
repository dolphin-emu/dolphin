// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <thread>

#include "Common/Event.h"
#include "Common/SPSCQueue.h"
#include "Common/Thread.h"

namespace Common
{
namespace detail
{
template <typename T, bool IsSingleProducer>
class WorkQueueThreadBase final
{
public:
  using FunctionType = std::function<void(T)>;

  WorkQueueThreadBase() = default;
  WorkQueueThreadBase(std::string name, FunctionType function)
  {
    Reset(std::move(name), std::move(function));
  }
  ~WorkQueueThreadBase() { Shutdown(); }

  // Shuts the current work thread down (if any) and starts a new thread with the given function
  // Note: Some consumers of this API push items to the queue before starting the thread.
  void Reset(std::string name, FunctionType function)
  {
    auto lg = GetLockGuard();
    Shutdown();
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

    // Fast path avoids round trip thread communication and saves ~20us.
    if (m_items.Empty())
      return;

    RunCommand([&] { m_items.Clear(); });
  }

  // Tells the worker thread to stop when its queue is empty.
  // Blocks until the worker thread exits. Does nothing if thread isn't running.
  void Shutdown()
  {
    auto lg = GetLockGuard();
    WaitForCompletion();
    StopThread();
  }

  // Tells the worker thread to stop immediately, potentially leaving work in the queue.
  // Blocks until the worker thread exits. Does nothing if thread isn't running.
  void Stop()
  {
    auto lg = GetLockGuard();
    StopThread();
  }

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
  using CommandFunction = std::function<void()>;

  // Blocking.
  void RunCommand(CommandFunction cmd)
  {
    if (!IsRunning())
    {
      std::invoke(cmd);
      return;
    }

    m_commands.Emplace(std::move(cmd));
    m_event.Set();
    m_commands.WaitForEmpty();
  }

  // Stop immediately.
  void StopThread()
  {
    if (!m_thread.joinable())
      return;

    // empty-function shutdown signal.
    m_commands.Emplace(CommandFunction{});
    m_event.Set();
    m_thread.join();
    m_commands.Clear();
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

  void ThreadLoop(const std::string& thread_name, const FunctionType& function)
  {
    Common::SetCurrentThreadName(thread_name.c_str());

    while (true)
    {
      while (!m_commands.Empty())
      {
        CommandFunction& command = m_commands.Front();
        // empty-function shutdown signal.
        if (!command)
          return;

        std::invoke(command);
        m_commands.Pop();
      }

      if (m_items.Empty())
      {
        m_event.Wait();
        continue;
      }

      function(std::move(m_items.Front()));
      m_items.Pop();
    }
  }

  std::thread m_thread;
  Common::WaitableSPSCQueue<T> m_items;
  Common::WaitableSPSCQueue<CommandFunction> m_commands;
  Common::Event m_event;

  using DummyMutex = std::type_identity<void>;
  using ProducerMutex = std::conditional_t<IsSingleProducer, DummyMutex, std::recursive_mutex>;
  ProducerMutex m_mutex;
};

// A WorkQueueThread-like class that takes functions to invoke.
template <template <typename> typename WorkThread>
class AsyncWorkThreadBase
{
public:
  using FuncType = std::function<void()>;

  AsyncWorkThreadBase() = default;
  explicit AsyncWorkThreadBase(std::string thread_name) { Reset(std::move(thread_name)); }

  void Reset(std::string thread_name)
  {
    m_worker.Reset(std::move(thread_name), std::invoke<FuncType>);
  }

  void Push(FuncType func) { m_worker.Push(std::move(func)); }

  auto PushBlocking(FuncType func)
  {
    std::packaged_task task{std::move(func)};
    m_worker.EmplaceItem([&] { task(); });
    return task.get_future().get();
  }

  void Cancel() { m_worker.Cancel(); }
  void Shutdown() { m_worker.Shutdown(); }
  void WaitForCompletion() { m_worker.WaitForCompletion(); }

private:
  WorkThread<FuncType> m_worker;
};
}  // namespace detail

// Multiple threads may use the public interface.
template <typename T>
using WorkQueueThread = detail::WorkQueueThreadBase<T, false>;

// A "Single Producer" WorkQueueThread.
// It uses no mutex but only one thread can safely manipulate the queue.
template <typename T>
using WorkQueueThreadSP = detail::WorkQueueThreadBase<T, true>;

using AsyncWorkThread = detail::AsyncWorkThreadBase<WorkQueueThread>;
using AsyncWorkThreadSP = detail::AsyncWorkThreadBase<WorkQueueThreadSP>;

}  // namespace Common
