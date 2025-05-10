// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
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

  void Initialize(u64 max_memory_allowed);
  void Shutdown();

  // Returns a vector of asset handles that were loaded
  std::vector<std::size_t> TakeLoadedAssetHandles();

  // Schedules assets on the worker threads to load
  void ScheduleAssetsToLoad(const std::list<CustomAsset*>& pending_assets);

  void SetAssetMemoryUsed(u64 memory_used);
  void Reset(bool restart_worker_threads = true);

private:
  bool StartWorkerThreads(u32 num_worker_threads);
  bool ResizeWorkerThreads(u32 num_worker_threads);
  bool HasWorkerThreads() const;
  void StopWorkerThreads();

  void WorkerThreadRun();

  Common::Flag m_exit_flag;

  u64 m_max_memory_allowed = 0;
  std::vector<std::thread> m_worker_threads;

  std::list<CustomAsset*> m_assets_to_load;
  std::atomic<u64> m_asset_memory_used = 0;
  std::chrono::steady_clock::time_point m_last_request_time = {};
  std::mutex m_assets_to_load_lock;

  std::condition_variable m_worker_thread_wake;

  std::vector<std::size_t> m_asset_handles_loaded;
  std::atomic<u64> m_asset_memory_loaded = 0;
  std::mutex m_assets_loaded_lock;
};
}  // namespace VideoCommon
