// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomResourceManager.h"

#include <fmt/format.h>

#include "Common/MemoryUtil.h"

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/VideoEvents.h"

namespace VideoCommon
{
void CustomResourceManager::Initialize()
{
  const size_t sys_mem = Common::MemPhysical();
  const size_t recommended_min_mem = 2 * size_t(1024 * 1024 * 1024);
  // keep 2GB memory for system stability if system RAM is 4GB+ - use half of memory in other cases
  m_max_ram_available =
      (sys_mem / 2 < recommended_min_mem) ? (sys_mem / 2) : (sys_mem - recommended_min_mem);

  m_asset_loader.Initialize(m_max_ram_available);

  m_xfb_event =
      AfterFrameEvent::Register([this](Core::System&) { XFBTriggered(); }, "CustomResourceManager");
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
  m_asset_handle_to_data.clear();
  m_asset_id_to_handle.clear();
  m_texture_data_asset_cache.clear();
  m_assets_to_reload.clear();
  m_ram_used = 0;
}

void CustomResourceManager::ReloadAsset(const CustomAssetLibrary::AssetID& asset_id)
{
  std::lock_guard guard(m_reload_mutex);
  m_assets_to_reload.insert(asset_id);
}

CustomTextureData* CustomResourceManager::GetTextureDataFromAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto [it, inserted] =
      m_texture_data_asset_cache.try_emplace(asset_id, InternalTextureDataResource{});
  if (it->second.asset_data &&
      it->second.asset_data->load_status == AssetData::LoadStatus::LoadFinalized)
  {
    m_loaded_assets.MarkAssetAsRecent(it->second.asset->GetHandle(), it->second.asset);
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
        &m_asset_handle_to_data[internal_texture_data->asset->GetHandle()];
  }

  auto texture_data = internal_texture_data->asset->GetData();
  if (!texture_data ||
      internal_texture_data->asset_data->load_status == AssetData::LoadStatus::PendingReload)
  {
    // Tell the system we are still interested in loading this asset
    const auto asset_handle = internal_texture_data->asset->GetHandle();
    m_pending_assets.MarkAssetAsRecent(asset_handle,
                                       m_asset_handle_to_data[asset_handle].asset.get());
  }
  else if (internal_texture_data->asset_data->load_status == AssetData::LoadStatus::LoadFinished)
  {
    internal_texture_data->texture_data = std::move(texture_data);
    internal_texture_data->asset_data->load_status = AssetData::LoadStatus::LoadFinalized;
  }
}

void CustomResourceManager::XFBTriggered()
{
  ProcessAssetsToReload();
  ProcessLoadedAssets();

  if (m_ram_used > m_max_ram_available)
  {
    RemoveAssetsUntilBelowMemoryLimit();
  }

  if (m_pending_assets.IsEmpty())
    return;
  m_asset_loader.SetAssetMemoryUsed(m_ram_used);
  if (m_ram_used < m_max_ram_available)
    m_asset_loader.ScheduleAssetsToLoad(m_pending_assets.Elements());
}

void CustomResourceManager::ProcessAssetsToReload()
{
  decltype(m_assets_to_reload) assets_to_reload;

  if (m_reload_mutex.try_lock())
  {
    std::swap(assets_to_reload, m_assets_to_reload);
    m_reload_mutex.unlock();
  }

  const auto now = std::chrono::steady_clock::now();
  for (const auto& asset_id : assets_to_reload)
  {
    if (const auto it = m_asset_id_to_handle.find(asset_id); it != m_asset_id_to_handle.end())
    {
      const auto asset_handle = it->second;
      AssetData& asset_data = m_asset_handle_to_data[asset_handle];
      asset_data.load_status = AssetData::LoadStatus::PendingReload;
      asset_data.has_errors = false;
      asset_data.load_request_time = now;
      for (const auto owner_handle : asset_data.asset_owners)
      {
        AssetData& owner_asset_data = m_asset_handle_to_data[owner_handle];
        if (owner_asset_data.load_status == AssetData::LoadStatus::LoadFinalized)
        {
          owner_asset_data.load_status = AssetData::LoadStatus::DependenciesChanged;
        }
      }
      m_pending_assets.MarkAssetAsAvailable(it->second, asset_data.asset.get());
    }
  }
}

void CustomResourceManager::ProcessLoadedAssets()
{
  const auto asset_handles_loaded = m_asset_loader.TakeLoadedAssetHandles();
  for (const std::size_t asset_handle : asset_handles_loaded)
  {
    AssetData& asset_data = m_asset_handle_to_data[asset_handle];

    // If we have a reload request that is newer than our loaded time
    // we need to wait
    if (asset_data.load_request_time > asset_data.asset->GetLastLoadedTime())
      continue;

    m_pending_assets.RemoveAsset(asset_handle);
    m_loaded_assets.MarkAssetAsAvailable(asset_handle, asset_data.asset.get());
    asset_data.load_status = AssetData::LoadStatus::LoadFinished;
    asset_data.load_request_time = {};
    m_ram_used += asset_data.asset->GetByteSizeInMemory();

    for (const auto owner_handle : asset_data.asset_owners)
    {
      AssetData& owner_asset_data = m_asset_handle_to_data[owner_handle];
      if (owner_asset_data.load_status == AssetData::LoadStatus::LoadFinalized)
      {
        owner_asset_data.load_status = AssetData::LoadStatus::DependenciesChanged;
      }
    }
  }
}

void CustomResourceManager::RemoveAssetsUntilBelowMemoryLimit()
{
  const u64 threshold_ram = 0.8f * m_max_ram_available;
  u64 ram_used = m_ram_used;

  // Clear out least recently used resources until
  // we get safely in our threshold
  while (ram_used > threshold_ram && m_loaded_assets.Size() > 0)
  {
    const auto asset = m_loaded_assets.RemoveLeastRecentAsset();
    ram_used -= asset->GetByteSizeInMemory();

    AssetData& asset_data = m_asset_handle_to_data[asset->GetHandle()];

    if (asset_data.type == AssetData::AssetType::TextureData)
    {
      m_texture_data_asset_cache.erase(asset->GetAssetId());
    }
    asset_data.asset->Unload();
    asset_data.load_status = AssetData::LoadStatus::Unloaded;
    asset_data.has_errors = false;
    asset_data.load_request_time = {};
  }

  // Recalculate to ensure accuracy
  m_ram_used = 0;
  for (const auto asset : m_loaded_assets.Elements())
  {
    m_ram_used += asset->GetByteSizeInMemory();
  }
}

}  // namespace VideoCommon
