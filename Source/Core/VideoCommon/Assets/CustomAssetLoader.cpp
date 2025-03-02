// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomAssetLoader.h"

#include <fmt/format.h>

#include "Common/Logging/Log.h"
#include "Common/Thread.h"

#include "UICommon/UICommon.h"

namespace VideoCommon
{
void CustomAssetLoader::Initialize()
{
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
    m_worker_threads.emplace_back(&CustomAssetLoader::WorkerThreadRun, this, i);
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

void CustomAssetLoader::WorkerThreadRun(u32 thread_index)
{
  Common::SetCurrentThreadName(fmt::format("Asset Loader {}", thread_index).c_str());

  std::unique_lock load_lock(m_assets_to_load_lock);
  while (true)
  {
    m_worker_thread_wake.wait(load_lock,
                              [&] { return !m_assets_to_load.empty() || m_exit_flag.IsSet(); });

    if (m_exit_flag.IsSet())
      return;

    // If more memory than allowed has already been loaded, we will load nothing more
    //  until the next ScheduleAssetsToLoad from Manager.
    if (m_used_memory > m_allowed_memory)
    {
      m_assets_to_load.clear();
      continue;
    }

    auto* const item = m_assets_to_load.front();
    m_assets_to_load.pop_front();

    // Make sure another thread isn't loading this handle.
    if (!m_handles_in_progress.insert(item->GetHandle()).second)
      continue;

    load_lock.unlock();

    const bool load_successful = item->Load();

    load_lock.lock();

    {
      INFO_LOG_FMT(VIDEO, "CustomAssetLoader thread {} loaded: {} ({})", thread_index,
                   item->GetAssetId(), UICommon::FormatSize(item->GetByteSizeInMemory()));

      std::lock_guard lk{m_assets_loaded_lock};
      if (load_successful)
        m_used_memory += item->GetByteSizeInMemory();
      m_asset_handles_loaded.emplace_back(item->GetHandle(), load_successful);

      // Make sure no other threads try to re-process this item.
      // Manager will take the handles and re-ScheduleAssetsToLoad based on timestamps if needed.
      std::erase(m_assets_to_load, item);
    }

    m_handles_in_progress.erase(item->GetHandle());
  }
}

std::vector<CustomAssetLoader::AssetHandleLoadedPair> CustomAssetLoader::TakeLoadedAssetHandles()
{
  std::lock_guard guard(m_assets_loaded_lock);
  m_used_memory = 0;
  return std::move(m_asset_handles_loaded);
}

void CustomAssetLoader::ScheduleAssetsToLoad(std::list<CustomAsset*> assets_to_load,
                                             u64 allowed_memory)
{
  if (assets_to_load.empty()) [[unlikely]]
    return;

  // There's new assets to process, notify worker threads
  std::lock_guard guard(m_assets_to_load_lock);
  m_allowed_memory = allowed_memory;
  m_assets_to_load = std::move(assets_to_load);
  m_worker_thread_wake.notify_all();
}

void CustomAssetLoader::Reset(bool restart_worker_threads)
{
  const std::size_t worker_thread_count = m_worker_threads.size();
  StopWorkerThreads();

  m_assets_to_load.clear();
  m_allowed_memory = 0;
  m_asset_handles_loaded.clear();
  m_used_memory = 0;

  if (restart_worker_threads)
  {
    StartWorkerThreads(static_cast<u32>(worker_thread_count));
  }
}

}  // namespace VideoCommon
