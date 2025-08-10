// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>

#include "Common/Flag.h"
#include "Common/Functional.h"

struct EfbPokeData;
class PointerWrap;

class AsyncRequests
{
public:
  AsyncRequests();

  void PullEvents()
  {
    if (!m_empty.IsSet())
      PullEventsInternal();
  }
  void WaitForEmptyQueue();
  void SetEnable(bool enable);
  void SetPassthrough(bool enable);

  template <typename F>
  void PushEvent(F&& callback)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_passthrough)
    {
      std::invoke(callback);
      return;
    }

    QueueEvent(Event{std::forward<F>(callback)});
  }

  template <typename F>
  auto PushBlockingEvent(F&& callback) -> std::invoke_result_t<F>
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_passthrough)
      return std::invoke(callback);

    std::packaged_task task{std::forward<F>(callback)};
    QueueEvent(Event{[&] { task(); }});

    lock.unlock();
    return task.get_future().get();
  }

  static AsyncRequests* GetInstance() { return &s_singleton; }

private:
  using Event = Common::MoveOnlyFunction<void()>;

  void QueueEvent(Event&& event);

  void PullEventsInternal();

  static AsyncRequests s_singleton;

  Common::Flag m_empty;
  std::queue<Event> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_cond;

  bool m_enable = false;
  bool m_passthrough = true;
};
