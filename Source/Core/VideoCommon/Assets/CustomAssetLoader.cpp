// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomAssetLoader.h"

#include <set>

#include "Common/Thread.h"

namespace VideoCommon
{
void CustomAssetLoader::Initialize(u64 max_memory_allowed)
{
  m_max_memory_allowed = max_memory_allowed;
  ResizeWorkerThreads(2);
}

void CustomAssetLoader::Shutdown()
{
  Reset(false);
}

bool CustomAssetLoader::StartWorkerThreads(u32 num_worker_threads)
{
  for (u32 i = 0; i < num_worker_threads; i++)
  {
    m_worker_threads.emplace_back(&CustomAssetLoader::WorkerThreadRun, this);
  }

  return HasWorkerThreads();
}

bool CustomAssetLoader::ResizeWorkerThreads(u32 num_worker_threads)
{
  if (m_worker_threads.size() == num_worker_threads)
    return true;

  StopWorkerThreads();
  return StartWorkerThreads(num_worker_threads);
}

bool CustomAssetLoader::HasWorkerThreads() const
{
  return !m_worker_threads.empty();
}

void CustomAssetLoader::StopWorkerThreads()
{
  if (!HasWorkerThreads())
    return;

  // Signal worker threads to stop, and wake all of them.
  {
    std::lock_guard guard(m_assets_to_load_lock);
    m_exit_flag.Set();
    m_worker_thread_wake.notify_all();
  }

  // Wait for worker threads to exit.
  for (std::thread& thr : m_worker_threads)
    thr.join();
  m_worker_threads.clear();
  m_exit_flag.Clear();
}

void CustomAssetLoader::WorkerThreadRun()
{
  Common::SetCurrentThreadName("Asset Loader Worker");
  std::unique_lock load_lock(m_assets_to_load_lock);
  std::set<std::size_t> handles_in_progress;
  while (!m_exit_flag.IsSet())
  {
    m_worker_thread_wake.wait(load_lock);

    while (!m_assets_to_load.empty() && !m_exit_flag.IsSet())
    {
      const auto iter = m_assets_to_load.begin();
      const auto item = *iter;
      m_assets_to_load.erase(iter);
      const auto [it, inserted] = handles_in_progress.insert(item->GetHandle());
      const auto last_request_time = m_last_request_time;

      // Was the asset added by another call to 'ScheduleAssetsToLoad'
      // while a load for that asset is still in progress on a worker?
      if (!inserted)
        continue;

      if ((m_asset_memory_used + m_asset_memory_loaded) > m_max_memory_allowed)
        break;

      load_lock.unlock();

      // Prevent a second load from occurring when a load finishes after
      // a new asset request is triggered by 'ScheduleAssetsToLoad'
      if (last_request_time > item->GetLastLoadedTime() && item->Load())
      {
        std::lock_guard guard(m_assets_loaded_lock);
        m_asset_memory_loaded += item->GetByteSizeInMemory();
        m_asset_handles_loaded.push_back(item->GetHandle());
      }

      load_lock.lock();
      handles_in_progress.erase(item->GetHandle());
    }
  }
}

std::vector<std::size_t> CustomAssetLoader::TakeLoadedAssetHandles()
{
  std::vector<std::size_t> completed_asset_handles;
  {
    std::lock_guard guard(m_assets_loaded_lock);
    m_asset_handles_loaded.swap(completed_asset_handles);
    m_asset_memory_loaded = 0;
  }

  return completed_asset_handles;
}

void CustomAssetLoader::ScheduleAssetsToLoad(const std::list<CustomAsset*>& assets_to_load)
{
  if (assets_to_load.empty()) [[unlikely]]
    return;

  // There's new assets to process, notify worker threads
  {
    std::lock_guard guard(m_assets_to_load_lock);
    m_assets_to_load = assets_to_load;
    m_last_request_time = std::chrono::steady_clock::now();
    m_worker_thread_wake.notify_all();
  }
}

void CustomAssetLoader::SetAssetMemoryUsed(u64 memory_used)
{
  m_asset_memory_used = memory_used;
}

void CustomAssetLoader::Reset(bool restart_worker_threads)
{
  const std::size_t worker_thread_count = m_worker_threads.size();
  StopWorkerThreads();

  m_assets_to_load.clear();
  m_asset_memory_used = 0;
  m_asset_handles_loaded.clear();
  m_asset_memory_loaded = 0;

  if (restart_worker_threads)
  {
    StartWorkerThreads(static_cast<u32>(worker_thread_count));
  }
}

}  // namespace VideoCommon
