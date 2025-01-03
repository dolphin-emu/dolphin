// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomAssetLoader.h"

#include "Common/MemoryUtil.h"
#include "Common/Thread.h"

#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/VideoEvents.h"

namespace VideoCommon
{
void CustomAssetLoader::Init()
{
  m_frame_event =
      AfterFrameEvent::Register([this](Core::System&) { OnFrameEnd(); }, "CustomAssetLoader");

  const size_t sys_mem = Common::MemPhysical();
  const size_t recommended_min_mem = 2 * size_t(1024 * 1024 * 1024);
  // keep 2GB memory for system stability if system RAM is 4GB+ - use half of memory in other cases
  m_max_memory_available =
      (sys_mem / 2 < recommended_min_mem) ? (sys_mem / 2) : (sys_mem - recommended_min_mem);

  ResizeWorkerThreads(2);
}

void CustomAssetLoader ::Shutdown()
{
  Reset(false);
}

std::shared_ptr<GameTextureAsset>
CustomAssetLoader::LoadGameTexture(const CustomAssetLibrary::AssetID& asset_id,
                                   std::shared_ptr<CustomAssetLibrary> library)
{
  return LoadOrCreateAsset<GameTextureAsset>(asset_id, m_game_textures, std::move(library));
}

std::shared_ptr<PixelShaderAsset>
CustomAssetLoader::LoadPixelShader(const CustomAssetLibrary::AssetID& asset_id,
                                   std::shared_ptr<CustomAssetLibrary> library)
{
  return LoadOrCreateAsset<PixelShaderAsset>(asset_id, m_pixel_shaders, std::move(library));
}

std::shared_ptr<MaterialAsset>
CustomAssetLoader::LoadMaterial(const CustomAssetLibrary::AssetID& asset_id,
                                std::shared_ptr<CustomAssetLibrary> library)
{
  return LoadOrCreateAsset<MaterialAsset>(asset_id, m_materials, std::move(library));
}

std::shared_ptr<MeshAsset> CustomAssetLoader::LoadMesh(const CustomAssetLibrary::AssetID& asset_id,
                                                       std::shared_ptr<CustomAssetLibrary> library)
{
  return LoadOrCreateAsset<MeshAsset>(asset_id, m_meshes, std::move(library));
}

void CustomAssetLoader::AssetReferenced(u64 asset_session_id)
{
  auto& asset_load_info = m_asset_load_info[asset_session_id];
  asset_load_info.last_xfb_seen = m_xfbs_seen;
}

void CustomAssetLoader::Reset(bool restart_worker_threads)
{
  const std::size_t worker_thread_count = m_worker_threads.size();
  StopWorkerThreads();

  m_game_textures.clear();
  m_pixel_shaders.clear();
  m_materials.clear();
  m_meshes.clear();

  m_assetid_to_asset_index.clear();
  m_asset_load_info.clear();

  {
    std::lock_guard<std::mutex> guard(m_reload_work_lock);
    m_assetids_to_reload.clear();
  }

  {
    std::lock_guard<std::mutex> guard(m_pending_work_lock);
    m_pending_work_per_frame.clear();
  }

  {
    std::lock_guard<std::mutex> guard(m_completed_work_lock);
    m_completed_work.clear();
  }

  if (restart_worker_threads)
  {
    StartWorkerThreads(static_cast<u32>(worker_thread_count));
  }
}

void CustomAssetLoader::ReloadAsset(const CustomAssetLibrary::AssetID& asset_id)
{
  std::lock_guard<std::mutex> guard(m_reload_work_lock);
  m_assetids_to_reload.push_back(asset_id);
}

bool CustomAssetLoader::StartWorkerThreads(u32 num_worker_threads)
{
  if (num_worker_threads == 0)
    return true;

  for (u32 i = 0; i < num_worker_threads; i++)
  {
    m_worker_thread_start_result.store(false);

    void* thread_param = nullptr;
    std::thread thr(&CustomAssetLoader::WorkerThreadEntryPoint, this, thread_param);
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

void CustomAssetLoader::WorkerThreadEntryPoint(void* param)
{
  Common::SetCurrentThreadName("Asset Loader Worker");

  m_worker_thread_start_result.store(true);
  m_init_event.Set();

  WorkerThreadRun();
}

void CustomAssetLoader::WorkerThreadRun()
{
  std::unique_lock<std::mutex> pending_lock(m_pending_work_lock);
  while (!m_exit_flag.IsSet())
  {
    m_worker_thread_wake.wait(pending_lock);

    while (!m_pending_work_per_frame.empty() && !m_exit_flag.IsSet())
    {
      m_busy_workers++;

      auto pending_iter = m_pending_work_per_frame.begin();
      auto item(std::move(pending_iter->second));
      m_pending_work_per_frame.erase(pending_iter);

      const auto item_shared = item.lock();
      pending_lock.unlock();

      if (item_shared && m_last_frame_total_loaded_memory < m_max_memory_available)
      {
        if (item_shared->Load())
        {
          // This asset could be double counted, but will be corected on the next frame
          m_last_frame_total_loaded_memory += item_shared->GetByteSizeInMemory();
        }

        // Regardless of whether the load was successful or not
        // mark it as complete
        {
          std::lock_guard<std::mutex> completed_guard(m_completed_work_lock);
          m_completed_work.push_back(item_shared->GetSessionId());
        }
      }

      pending_lock.lock();
      m_busy_workers--;
    }
  }
}

void CustomAssetLoader::OnFrameEnd()
{
  std::vector<CustomAssetLibrary::AssetID> assetids_to_reload;
  {
    std::lock_guard<std::mutex> guard(m_reload_work_lock);
    m_assetids_to_reload.swap(assetids_to_reload);
  }
  for (const CustomAssetLibrary::AssetID& asset_id : assetids_to_reload)
  {
    if (const auto asset_session_id_iter = m_assetid_to_asset_index.find(asset_id);
        asset_session_id_iter != m_assetid_to_asset_index.end())
    {
      auto& asset_load_info = m_asset_load_info[asset_session_id_iter->second];
      asset_load_info.xfb_load_request = m_xfbs_seen;
    }
  }

  std::vector<std::size_t> completed_work;
  {
    std::lock_guard<std::mutex> guard(m_completed_work_lock);
    m_completed_work.swap(completed_work);
  }
  for (std::size_t completed_index : completed_work)
  {
    auto& asset_load_info = m_asset_load_info[completed_index];

    // If we had a load request and it wasn't from this frame, clear it
    if (asset_load_info.xfb_load_request && asset_load_info.xfb_load_request != m_xfbs_seen)
    {
      asset_load_info.xfb_load_request = std::nullopt;
    }
  }

  m_xfbs_seen++;

  std::size_t total_bytes_loaded = 0;

  // Build up the work prioritizing newest requested assets first
  PendingWorkContainer new_pending_work;
  for (const auto& asset_load_info : m_asset_load_info)
  {
    if (const auto asset = asset_load_info.asset.lock())
    {
      total_bytes_loaded += asset->GetByteSizeInMemory();
      if (total_bytes_loaded > m_max_memory_available)
      {
        if (!m_memory_exceeded)
        {
          m_memory_exceeded = true;
          ERROR_LOG_FMT(VIDEO,
                        "Asset memory exceeded with asset '{}', future assets won't load until "
                        "memory is available.",
                        asset->GetAssetId());
        }
        break;
      }
      if (asset_load_info.xfb_load_request)
      {
        new_pending_work.emplace(asset_load_info.last_xfb_seen, asset_load_info.asset);
      }
    }
  }

  if (m_memory_exceeded && total_bytes_loaded < m_max_memory_available)
  {
    INFO_LOG_FMT(VIDEO, "Asset memory went below limit, new assets can begin loading.");
    m_memory_exceeded = false;
  }

  m_last_frame_total_loaded_memory = total_bytes_loaded;

  if (new_pending_work.empty())
    return;

  // Now notify our workers
  {
    std::lock_guard<std::mutex> guard(m_pending_work_lock);
    std::swap(m_pending_work_per_frame, new_pending_work);
    m_worker_thread_wake.notify_all();
  }
}

}  // namespace VideoCommon
