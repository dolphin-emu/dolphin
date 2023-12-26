// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomAssetLoader.h"

#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

namespace VideoCommon
{
void CustomAssetLoader::Init()
{
  m_asset_monitor_thread_shutdown.Clear();

  const size_t sys_mem = Common::MemPhysical();
  const size_t recommended_min_mem = 2 * size_t(1024 * 1024 * 1024);
  // keep 2GB memory for system stability if system RAM is 4GB+ - use half of memory in other cases
  m_max_memory_available =
      (sys_mem / 2 < recommended_min_mem) ? (sys_mem / 2) : (sys_mem - recommended_min_mem);

  m_asset_monitor_thread = std::thread([this]() {
    Common::SetCurrentThreadName("Asset monitor");
    while (true)
    {
      if (m_asset_monitor_thread_shutdown.IsSet())
      {
        break;
      }

      std::this_thread::sleep_for(TIME_BETWEEN_ASSET_MONITOR_CHECKS);

      std::lock_guard lk(m_asset_load_lock);
      for (auto& [asset_id, asset_to_monitor] : m_assets_to_monitor)
      {
        if (auto ptr = asset_to_monitor.lock())
        {
          const auto write_time = ptr->GetLastWriteTime();
          if (write_time > ptr->GetLastLoadedTime())
          {
            (void)ptr->Load();
          }
        }
      }
    }
  });

  m_asset_load_thread.Reset("Custom Asset Loader", [this](std::weak_ptr<CustomAsset> asset) {
    if (auto ptr = asset.lock())
    {
      if (ptr->Load())
      {
        std::lock_guard lk(m_asset_load_lock);
        const std::size_t asset_memory_size = ptr->GetByteSizeInMemory();
        if (m_max_memory_available >= m_total_bytes_loaded + asset_memory_size)
        {
          m_total_bytes_loaded += asset_memory_size;
          m_assets_to_monitor.try_emplace(ptr->GetAssetId(), ptr);
        }
        else
        {
          ERROR_LOG_FMT(VIDEO, "Failed to load asset {} because there was not enough memory.",
                        ptr->GetAssetId());
        }
      }
    }
  });
}

void CustomAssetLoader ::Shutdown()
{
  m_asset_load_thread.Shutdown(true);

  m_asset_monitor_thread_shutdown.Set();
  m_asset_monitor_thread.join();
  m_assets_to_monitor.clear();
  m_total_bytes_loaded = 0;
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
}  // namespace VideoCommon
