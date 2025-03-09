// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "VideoCommon/Assets/CustomAsset.h"

namespace VideoCommon
{
class CustomAssetLoader2
{
public:
  CustomAssetLoader2() = default;
  ~CustomAssetLoader2() = default;
  CustomAssetLoader2(const CustomAssetLoader2&) = delete;
  CustomAssetLoader2(CustomAssetLoader2&&) = delete;
  CustomAssetLoader2& operator=(const CustomAssetLoader2&) = delete;
  CustomAssetLoader2& operator=(CustomAssetLoader2&&) = delete;

  void Initialize();
  void Shutdown();

  // Returns a vector of asset session ids that were loaded in the last frame
  std::vector<std::size_t> LoadAssets(const std::list<CustomAsset*>& pending_assets,
                                      u64 current_loaded_memory, u64 max_memory_allowed);

  void Reset(bool restart_worker_threads = true);

private:
  bool StartWorkerThreads(u32 num_worker_threads);
  bool ResizeWorkerThreads(u32 num_worker_threads);
  bool HasWorkerThreads() const;
  void StopWorkerThreads();

  void WorkerThreadEntryPoint(void* param);
  void WorkerThreadRun();

  Common::Flag m_exit_flag;
  Common::Event m_init_event;

  std::vector<std::thread> m_worker_threads;
  std::atomic_bool m_worker_thread_start_result{false};

  std::list<CustomAsset*> m_pending_assets;
  std::atomic<u64> m_current_asset_memory = 0;
  u64 m_max_memory_allowed = 0;
  std::mutex m_pending_work_lock;

  std::condition_variable m_worker_thread_wake;

  std::vector<std::size_t> m_completed_asset_session_ids;
  std::atomic<u64> m_completed_asset_memory = 0;
  std::mutex m_completed_work_lock;
};
}  // namespace VideoCommon
