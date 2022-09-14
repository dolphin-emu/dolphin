// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"

namespace VideoCommon
{
class AsyncShaderCompiler
{
public:
  class WorkItem
  {
  public:
    virtual ~WorkItem() = default;
    virtual bool Compile() = 0;
    virtual void Retrieve() = 0;
  };

  using WorkItemPtr = std::unique_ptr<WorkItem>;

  AsyncShaderCompiler();
  virtual ~AsyncShaderCompiler();

  template <typename T, typename... Params>
  static WorkItemPtr CreateWorkItem(Params&&... params)
  {
    return std::make_unique<T>(std::forward<Params>(params)...);
  }

  // Queues a new work item to the compiler threads. The lower the priority, the sooner
  // this work item will be compiled, relative to the other work items.
  void QueueWorkItem(WorkItemPtr item, u32 priority);
  void RetrieveWorkItems();
  bool HasPendingWork();
  bool HasCompletedWork();

  // Calls progress_callback periodically, with completed_items, and total_items.
  // Returns false if interrupted.
  bool WaitUntilCompletion(const std::function<void(size_t, size_t)>& progress_callback);

  // Needed because of calling virtual methods in shutdown procedure.
  bool StartWorkerThreads(u32 num_worker_threads);
  bool ResizeWorkerThreads(u32 num_worker_threads);
  bool HasWorkerThreads() const;
  void StopWorkerThreads();

protected:
  virtual bool WorkerThreadInitMainThread(void** param);
  virtual bool WorkerThreadInitWorkerThread(void* param);
  virtual void WorkerThreadExit(void* param);

private:
  void WorkerThreadEntryPoint(void* param);
  void WorkerThreadRun();

  Common::Flag m_exit_flag;
  Common::Event m_init_event;

  std::vector<std::thread> m_worker_threads;
  std::atomic_bool m_worker_thread_start_result{false};

  // A multimap is used to store the work items. We can't use a priority_queue here, because
  // there's no way to obtain a non-const reference, which we need for the unique_ptr.
  std::multimap<u32, WorkItemPtr> m_pending_work;
  std::mutex m_pending_work_lock;
  std::condition_variable m_worker_thread_wake;
  std::atomic_size_t m_busy_workers{0};

  std::deque<WorkItemPtr> m_completed_work;
  std::mutex m_completed_work_lock;
};

}  // namespace VideoCommon
