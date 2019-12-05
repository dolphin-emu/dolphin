// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <queue>
#include <thread>

#include "Common/Event.h"
#include "Common/Flag.h"

// A thread that executes the given function for every item placed into its queue.

namespace Common
{
template <typename T>
class WorkQueueThread
{
public:
  WorkQueueThread() = default;
  WorkQueueThread(std::function<void(T)> function) { Reset(std::move(function)); }
  ~WorkQueueThread() { Shutdown(); }
  void Reset(std::function<void(T)> function)
  {
    Shutdown();
    m_shutdown.Clear();
    m_function = std::move(function);
    m_thread = std::thread(&WorkQueueThread::ThreadLoop, this);
  }

  template <typename... Args>
  void EmplaceItem(Args&&... args)
  {
    {
      std::lock_guard lg(m_lock);
      m_items.emplace(std::forward<Args>(args)...);
    }
    m_wakeup.Set();
  }

private:
  void Shutdown()
  {
    if (m_thread.joinable())
    {
      m_shutdown.Set();
      m_wakeup.Set();
      m_thread.join();
    }
  }

  void ThreadLoop()
  {
    while (true)
    {
      m_wakeup.Wait();

      while (true)
      {
        std::unique_lock lg(m_lock);
        if (m_items.empty())
          break;
        T item{std::move(m_items.front())};
        m_items.pop();
        lg.unlock();

        m_function(std::move(item));
      }

      if (m_shutdown.IsSet())
        break;
    }
  }

  std::function<void(T)> m_function;
  std::thread m_thread;
  Common::Event m_wakeup;
  Common::Flag m_shutdown;
  std::mutex m_lock;
  std::queue<T> m_items;
};

}  // namespace Common
