// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomAssetLoader2.h"

#include "Common/Logging/Log.h"
#include "Common/Thread.h"

namespace VideoCommon
{
void CustomAssetLoader2::Initialize()
{
  ResizeWorkerThreads(2);
}

void CustomAssetLoader2 ::Shutdown()
{
  Reset(false);
}

void CustomAssetLoader2::Reset(bool restart_worker_threads)
{
  const std::size_t worker_thread_count = m_worker_threads.size();
  StopWorkerThreads();

  {
    std::lock_guard<std::mutex> guard(m_pending_work_lock);
    m_pending_assets.clear();
    m_max_memory_allowed = 0;
    m_current_asset_memory = 0;
  }

  {
    std::lock_guard<std::mutex> guard(m_completed_work_lock);
    m_completed_asset_session_ids.clear();
    m_completed_asset_memory = 0;
  }

  if (restart_worker_threads)
  {
    StartWorkerThreads(static_cast<u32>(worker_thread_count));
  }
}

bool CustomAssetLoader2::StartWorkerThreads(u32 num_worker_threads)
{
  if (num_worker_threads == 0)
    return true;

  for (u32 i = 0; i < num_worker_threads; i++)
  {
    m_worker_thread_start_result.store(false);

    void* thread_param = nullptr;
    std::thread thr(&CustomAssetLoader2::WorkerThreadEntryPoint, this, thread_param);
    m_init_event.Wait();

    if (!m_worker_thread_start_result.load())
    {
      WARN_LOG_FMT(VIDEO, "Failed to start asset load worker thread.");
      thr.join();
      break;
    }

    m_worker_threads.push_back(std::move(thr));
  }

  return HasWorkerThreads();
}

bool CustomAssetLoader2::ResizeWorkerThreads(u32 num_worker_threads)
{
  if (m_worker_threads.size() == num_worker_threads)
    return true;

  StopWorkerThreads();
  return StartWorkerThreads(num_worker_threads);
}

bool CustomAssetLoader2::HasWorkerThreads() const
{
  return !m_worker_threads.empty();
}

void CustomAssetLoader2::StopWorkerThreads()
{
  if (!HasWorkerThreads())
    return;

  // Signal worker threads to stop, and wake all of them.
  {
    std::lock_guard<std::mutex> guard(m_pending_work_lock);
    m_exit_flag.Set();
    m_worker_thread_wake.notify_all();
  }

  // Wait for worker threads to exit.
  for (std::thread& thr : m_worker_threads)
    thr.join();
  m_worker_threads.clear();
  m_exit_flag.Clear();
}

void CustomAssetLoader2::WorkerThreadEntryPoint(void* param)
{
  Common::SetCurrentThreadName("Asset Loader Worker");

  m_worker_thread_start_result.store(true);
  m_init_event.Set();

  WorkerThreadRun();
}

void CustomAssetLoader2::WorkerThreadRun()
{
  std::unique_lock<std::mutex> pending_lock(m_pending_work_lock);
  while (!m_exit_flag.IsSet())
  {
    m_worker_thread_wake.wait(pending_lock);

    while (!m_pending_assets.empty() && !m_exit_flag.IsSet())
    {
      auto pending_iter = m_pending_assets.begin();
      const auto item = *pending_iter;
      m_pending_assets.erase(pending_iter);

      if ((m_current_asset_memory + m_completed_asset_memory) > m_max_memory_allowed)
        break;

      pending_lock.unlock();
      if (item->Load())
      {
        std::lock_guard<std::mutex> completed_guard(m_completed_work_lock);
        m_completed_asset_memory += item->GetByteSizeInMemory();
        m_completed_asset_session_ids.push_back(item->GetSessionId());
      }

      pending_lock.lock();
    }
  }
}

std::vector<std::size_t>
CustomAssetLoader2::LoadAssets(const std::list<CustomAsset*>& pending_assets,
                               u64 current_loaded_memory, u64 max_memory_allowed)
{
  u64 total_memory = current_loaded_memory;
  std::vector<std::size_t> completed_asset_session_ids;
  {
    std::lock_guard<std::mutex> guard(m_completed_work_lock);
    m_completed_asset_session_ids.swap(completed_asset_session_ids);
    total_memory += m_completed_asset_memory;
    m_completed_asset_memory = 0;
  }

  if (pending_assets.empty())
    return completed_asset_session_ids;

  if (total_memory > max_memory_allowed)
    return completed_asset_session_ids;

  // There's new assets to process, notify worker threads
  {
    std::lock_guard<std::mutex> guard(m_pending_work_lock);
    m_pending_assets = pending_assets;
    m_current_asset_memory = total_memory;
    m_max_memory_allowed = max_memory_allowed;
    if (m_current_asset_memory < m_max_memory_allowed)
      m_worker_thread_wake.notify_all();
  }

  return completed_asset_session_ids;
}

}  // namespace VideoCommon
