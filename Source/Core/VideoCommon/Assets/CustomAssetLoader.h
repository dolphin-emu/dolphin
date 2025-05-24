// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "Common/Flag.h"
#include "VideoCommon/Assets/CustomAsset.h"

namespace VideoCommon
{
// This class takes any number of assets
// and loads them across a configurable
// thread pool
class CustomAssetLoader
{
public:
  CustomAssetLoader() = default;
  ~CustomAssetLoader() = default;
  CustomAssetLoader(const CustomAssetLoader&) = delete;
  CustomAssetLoader(CustomAssetLoader&&) = delete;
  CustomAssetLoader& operator=(const CustomAssetLoader&) = delete;
  CustomAssetLoader& operator=(CustomAssetLoader&&) = delete;

  void Initialize();
  void Shutdown();

  using AssetHandleLoadedPair = std::pair<std::size_t, bool>;
  // Returns a vector of loaded asset handle / loaded result pairs
  std::vector<AssetHandleLoadedPair> TakeLoadedAssetHandles();

  // Schedule assets to load on the worker threads
  //  and set how much memory is available for loading these additional assets.
  void ScheduleAssetsToLoad(std::list<CustomAsset*> assets_to_load, u64 allowed_memory);

  void Reset(bool restart_worker_threads = true);

private:
  bool StartWorkerThreads(u32 num_worker_threads);
  bool ResizeWorkerThreads(u32 num_worker_threads);
  bool HasWorkerThreads() const;
  void StopWorkerThreads();

  void WorkerThreadRun(u32 thread_index);

  Common::Flag m_exit_flag;

  std::vector<std::thread> m_worker_threads;

  std::mutex m_assets_to_load_lock;
  std::list<CustomAsset*> m_assets_to_load;

  std::condition_variable m_worker_thread_wake;

  std::vector<AssetHandleLoadedPair> m_asset_handles_loaded;

  // Memory available to load new assets.
  u64 m_allowed_memory = 0;

  // Memory used by just-loaded assets yet to be taken by the Manager.
  std::atomic<u64> m_used_memory = 0;

  std::mutex m_assets_loaded_lock;

  std::set<std::size_t> m_handles_in_progress;
};
}  // namespace VideoCommon
