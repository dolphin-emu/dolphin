// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomResourceManager.h"

#include <fmt/format.h>

#include "Common/MathUtil.h"
#include "Common/MemoryUtil.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoEvents.h"

namespace VideoCommon
{
void CustomResourceManager::Initialize()
{
  m_asset_loader.Initialize();

  const size_t sys_mem = Common::MemPhysical();
  const size_t recommended_min_mem = 2 * size_t(1024 * 1024 * 1024);
  // keep 2GB memory for system stability if system RAM is 4GB+ - use half of memory in other cases
  m_max_ram_available =
      (sys_mem / 2 < recommended_min_mem) ? (sys_mem / 2) : (sys_mem - recommended_min_mem);

  m_xfb_event = AfterFrameEvent::Register([this](Core::System&) { XFBTriggered(""); },
                                          "CustomResourceManager");
}

void CustomResourceManager::Shutdown()
{
  Reset();

  m_asset_loader.Shutdown();
}

void CustomResourceManager::Reset()
{
  m_asset_loader.Reset(true);

  m_loaded_assets = {};
  m_pending_assets = {};
  m_session_id_to_asset_data.clear();
  m_asset_id_to_session_id.clear();
  m_ram_used = 0;
}

void CustomResourceManager::ReloadAsset(const CustomAssetLibrary::AssetID& asset_id)
{
  std::lock_guard<std::mutex> guard(m_reload_mutex);
  m_assets_to_reload.insert(asset_id);
}

CustomTextureData* CustomResourceManager::GetTextureDataFromAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto [it, inserted] =
      m_texture_data_asset_cache.try_emplace(asset_id, InternalTextureDataResource{});
  if (it->second.asset_data &&
      it->second.asset_data->load_type == AssetData::LoadType::LoadFinalyzed)
  {
    m_loaded_assets.put(it->second.asset->GetSessionId(), it->second.asset);
    return &it->second.texture_data->m_texture;
  }

  LoadTextureDataAsset(asset_id, std::move(library), &it->second);

  return nullptr;
}

void CustomResourceManager::LoadTextureDataAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
    InternalTextureDataResource* internal_texture_data)
{
  if (!internal_texture_data->asset)
  {
    internal_texture_data->asset =
        CreateAsset<GameTextureAsset>(asset_id, AssetData::AssetType::TextureData, library);
    internal_texture_data->asset_data =
        &m_session_id_to_asset_data[internal_texture_data->asset->GetSessionId()];
  }

  auto texture_data = internal_texture_data->asset->GetData();
  if (!texture_data ||
      internal_texture_data->asset_data->load_type == AssetData::LoadType::PendingReload)
  {
    // Tell the system we are still interested in loading this asset
    const auto session_id = internal_texture_data->asset->GetSessionId();
    m_pending_assets.put(session_id, m_session_id_to_asset_data[session_id].asset.get());
  }
  else if (internal_texture_data->asset_data->load_type == AssetData::LoadType::LoadFinished)
  {
    internal_texture_data->texture_data = std::move(texture_data);
    internal_texture_data->asset_data->load_type = AssetData::LoadType::LoadFinalyzed;
  }
}

void CustomResourceManager::XFBTriggered(std::string_view)
{
  std::set<std::size_t> session_ids_reloaded_this_frame;

  // Look for any assets requested to be reloaded
  {
    decltype(m_assets_to_reload) assets_to_reload;

    if (m_reload_mutex.try_lock())
    {
      std::swap(assets_to_reload, m_assets_to_reload);
      m_reload_mutex.unlock();
    }

    for (const auto& asset_id : assets_to_reload)
    {
      if (const auto it = m_asset_id_to_session_id.find(asset_id);
          it != m_asset_id_to_session_id.end())
      {
        const auto session_id = it->second;
        session_ids_reloaded_this_frame.insert(session_id);
        AssetData& asset_data = m_session_id_to_asset_data[session_id];
        asset_data.load_type = AssetData::LoadType::PendingReload;
        asset_data.has_errors = false;
        for (const auto owner_session_id : asset_data.asset_owners)
        {
          AssetData& owner_asset_data = m_session_id_to_asset_data[owner_session_id];
          if (owner_asset_data.load_type == AssetData::LoadType::LoadFinalyzed)
          {
            owner_asset_data.load_type = AssetData::LoadType::DependenciesChanged;
          }
        }
        m_pending_assets.put(it->second, asset_data.asset.get());
      }
    }
  }

  if (m_ram_used > m_max_ram_available)
  {
    const u64 threshold_ram = 0.8f * m_max_ram_available;
    u64 ram_used = m_ram_used;

    // Clear out least recently used resources until
    // we get safely in our threshold
    while (ram_used > threshold_ram && m_loaded_assets.size() > 0)
    {
      const auto asset = m_loaded_assets.pop();
      ram_used -= asset->GetByteSizeInMemory();

      AssetData& asset_data = m_session_id_to_asset_data[asset->GetSessionId()];

      if (asset_data.type == AssetData::AssetType::TextureData)
      {
        m_texture_data_asset_cache.erase(asset->GetAssetId());
      }
      asset_data.asset.reset();
      asset_data.load_type = AssetData::LoadType::Unloaded;
    }

    // Recalculate to ensure accuracy
    m_ram_used = 0;
    for (const auto asset : m_loaded_assets.elements())
    {
      m_ram_used += asset->GetByteSizeInMemory();
    }
  }

  if (m_pending_assets.empty())
    return;

  const auto asset_session_ids_loaded =
      m_asset_loader.LoadAssets(m_pending_assets.elements(), m_ram_used, m_max_ram_available);
  for (const std::size_t session_id : asset_session_ids_loaded)
  {
    // While unlikely, if we loaded an asset in the previous frame but it was reloaded
    // this frame, we should ignore this load and wait on the reload
    if (session_ids_reloaded_this_frame.count(session_id) > 0) [[unlikely]]
      continue;

    m_pending_assets.erase(session_id);

    AssetData& asset_data = m_session_id_to_asset_data[session_id];
    m_loaded_assets.put(session_id, asset_data.asset.get());
    asset_data.load_type = AssetData::LoadType::LoadFinished;
    m_ram_used += asset_data.asset->GetByteSizeInMemory();

    for (const auto owner_session_id : asset_data.asset_owners)
    {
      AssetData& owner_asset_data = m_session_id_to_asset_data[owner_session_id];
      if (owner_asset_data.load_type == AssetData::LoadType::LoadFinalyzed)
      {
        owner_asset_data.load_type = AssetData::LoadType::DependenciesChanged;
      }
    }
  }
}

}  // namespace VideoCommon
