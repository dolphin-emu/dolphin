// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <queue>
#include <thread>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Thread.h"

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
    std::lock_guard lg(m_lock);
    m_cancelled = false;
    m_function = std::move(function);
    m_thread = std::thread(&WorkQueueThread::ThreadLoop, this);
  }

  template <typename... Args>
  void EmplaceItem(Args&&... args)
  {
    std::lock_guard lg(m_lock);
    if (m_shutdown)
    {
      return;
    }
    m_items.emplace(std::forward<Args>(args)...);
    m_idle = false;
    m_worker_cond_var.notify_one();
  }

  void Push(T&& item)
  {
    std::lock_guard lg(m_lock);
    if (m_cancelled)
    {
      return;
    }
    m_items.push(item);
    m_idle = false;
    m_worker_cond_var.notify_one();
  }

  void Push(const T& item)
  {
      std::lock_guard lg(m_lock);
      if (m_cancelled)
      {
        return;
      }
      m_items.push(item);
      m_idle = false;
      m_worker_cond_var.notify_one();
  }

  void Clear()
  {
    std::lock_guard lg(m_lock);
    m_items = std::queue<T>();
    m_worker_cond_var.notify_one();
  }

  void Cancel()
  {
    if (!m_thread.joinable())
    {
      return;
    }

    {
      std::unique_lock lg(m_lock);
      m_items = std::queue<T>();
      m_cancelled = true;
      m_shutdown = true;
      m_worker_cond_var.notify_one();
    }
    m_thread.join();
  }

  void Shutdown()
  {
    if (!m_thread.joinable())
    {
      return;
    }

    {
      std::unique_lock lg(m_lock);
      m_shutdown = true;
      m_worker_cond_var.notify_one();
    }
    m_thread.join();
  }

  // Doesn't return until the most recent function invocation has finished.
  void ClearAndFlush()
  {
    if (!m_thread.joinable())
    {
      return;
    }

    std::unique_lock lg(m_lock);
    m_items = std::queue<T>();
    m_wait_cond_var.wait(lg, [&] {
        return m_idle;
    });
  }

  // Doesn't return until the most recent function invocation has finished.
  void Flush()
  {
    if (!m_thread.joinable())
    {
      return;
    }

    std::unique_lock lg(m_lock);
    m_wait_cond_var.wait(lg, [&] {
        return m_idle;
    });
  }

private:
  void ThreadLoop()
  {
    Common::SetCurrentThreadName("WorkQueueThread");

    while (true)
    {
      std::unique_lock lg(m_lock);
      if (m_items.empty())
      {
        m_idle = true;
        m_wait_cond_var.notify_all();
        m_worker_cond_var.wait(lg, [&] {
            return m_shutdown || !m_items.empty();
        });
        if (m_shutdown)
        {
          break;
        }
        continue;
      }
      T item{std::move(m_items.front())};
      m_items.pop();
      lg.unlock();

      m_function(std::move(item));
    }
  }

  std::function<void(T)> m_function;
  std::thread m_thread;
  std::mutex m_lock;
  std::queue<T> m_items;
  std::condition_variable m_wait_cond_var;
  std::condition_variable m_worker_cond_var;
  bool m_idle = true;
  bool m_shutdown = false;
  bool m_cancelled = false;
};

}  // namespace Common
