// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/HookableEvent.h"
#include "Common/Logging/Log.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/Present.h"

namespace VideoCommon
{
// This class is responsible for loading data asynchronously when requested
// and watches that data asynchronously reloading it if it changes
class CustomAssetLoader
{
public:
  CustomAssetLoader() = default;
  ~CustomAssetLoader() = default;
  CustomAssetLoader(const CustomAssetLoader&) = delete;
  CustomAssetLoader(CustomAssetLoader&&) = delete;
  CustomAssetLoader& operator=(const CustomAssetLoader&) = delete;
  CustomAssetLoader& operator=(CustomAssetLoader&&) = delete;

  void Init();
  void Shutdown();

  // The following Load* functions will load or create an asset associated
  // with the given asset id
  // Loads happen asynchronously where the data will be set now or in the future
  // Callees are expected to query the underlying data with 'GetData()'
  // from the 'CustomLoadableAsset' class to determine if the data is ready for use
  std::shared_ptr<GameTextureAsset> LoadGameTexture(const CustomAssetLibrary::AssetID& asset_id,
                                                    std::shared_ptr<CustomAssetLibrary> library);

  std::shared_ptr<PixelShaderAsset> LoadPixelShader(const CustomAssetLibrary::AssetID& asset_id,
                                                    std::shared_ptr<CustomAssetLibrary> library);

  std::shared_ptr<MaterialAsset> LoadMaterial(const CustomAssetLibrary::AssetID& asset_id,
                                              std::shared_ptr<CustomAssetLibrary> library);

  std::shared_ptr<MeshAsset> LoadMesh(const CustomAssetLibrary::AssetID& asset_id,
                                      std::shared_ptr<CustomAssetLibrary> library);

  // Notifies the asset system that an asset has been used
  void AssetReferenced(u64 asset_session_id);

  // Requests that an asset that exists be reloaded
  void ReloadAsset(const CustomAssetLibrary::AssetID& asset_id);

  void Reset(bool restart_worker_threads = true);

private:
  // TODO C++20: use a 'derived_from' concept against 'CustomAsset' when available
  template <typename AssetType>
  std::shared_ptr<AssetType>
  LoadOrCreateAsset(const CustomAssetLibrary::AssetID& asset_id,
                    std::map<CustomAssetLibrary::AssetID, std::weak_ptr<AssetType>>& asset_map,
                    std::shared_ptr<CustomAssetLibrary> library)
  {
    auto [it, inserted] = asset_map.try_emplace(asset_id);
    if (!inserted)
    {
      auto shared = it->second.lock();
      if (shared)
      {
        auto& asset_load_info = m_asset_load_info[shared->GetSessionId()];
        asset_load_info.last_xfb_seen = m_xfbs_seen;
        asset_load_info.xfb_load_request = m_xfbs_seen;
        return shared;
      }
    }

    const auto [index_it, index_inserted] = m_assetid_to_asset_index.try_emplace(asset_id, 0);
    if (index_inserted)
    {
      index_it->second = m_asset_load_info.size();
    }

    auto ptr = std::make_shared<AssetType>(std::move(library), asset_id, index_it->second);
    it->second = ptr;

    AssetLoadInfo* asset_load_info;
    if (index_inserted)
    {
      m_asset_load_info.emplace_back();
      asset_load_info = &m_asset_load_info.back();
    }
    else
    {
      asset_load_info = &m_asset_load_info[index_it->second];
    }

    asset_load_info->asset = ptr;
    asset_load_info->last_xfb_seen = m_xfbs_seen;
    asset_load_info->xfb_load_request = m_xfbs_seen;
    return ptr;
  }

  bool StartWorkerThreads(u32 num_worker_threads);
  bool ResizeWorkerThreads(u32 num_worker_threads);
  bool HasWorkerThreads() const;
  void StopWorkerThreads();

  void WorkerThreadEntryPoint(void* param);
  void WorkerThreadRun();

  void OnFrameEnd();

  Common::EventHook m_frame_event;

  std::map<CustomAssetLibrary::AssetID, std::weak_ptr<GameTextureAsset>> m_game_textures;
  std::map<CustomAssetLibrary::AssetID, std::weak_ptr<PixelShaderAsset>> m_pixel_shaders;
  std::map<CustomAssetLibrary::AssetID, std::weak_ptr<MaterialAsset>> m_materials;
  std::map<CustomAssetLibrary::AssetID, std::weak_ptr<MeshAsset>> m_meshes;

  std::map<CustomAssetLibrary::AssetID, std::size_t> m_assetid_to_asset_index;

  struct AssetLoadInfo
  {
    std::weak_ptr<CustomAsset> asset;
    u64 last_xfb_seen = 0;
    std::optional<u64> xfb_load_request;
  };
  std::vector<AssetLoadInfo> m_asset_load_info;
  u64 m_xfbs_seen = 0;

  std::size_t m_max_memory_available = 0;
  bool m_memory_exceeded = false;
  std::atomic_size_t m_last_frame_total_loaded_memory = 0;

  Common::Flag m_exit_flag;
  Common::Event m_init_event;

  std::vector<std::thread> m_worker_threads;
  std::atomic_bool m_worker_thread_start_result{false};

  using PendingWorkContainer = std::multimap<u64, std::weak_ptr<CustomAsset>, std::greater<>>;
  PendingWorkContainer m_pending_work_per_frame;
  std::mutex m_pending_work_lock;
  std::condition_variable m_worker_thread_wake;
  std::atomic_size_t m_busy_workers{0};

  std::vector<std::size_t> m_completed_work;
  std::mutex m_completed_work_lock;

  std::vector<CustomAssetLibrary::AssetID> m_assetids_to_reload;
  std::mutex m_reload_work_lock;
};
}  // namespace VideoCommon
