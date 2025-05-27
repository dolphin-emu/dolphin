// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomResourceManager.h"

#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/VideoEvents.h"

namespace VideoCommon
{
void CustomResourceManager::Initialize()
{
  // Use half of available system memory but leave at least 2GiB unused for system stability.
  constexpr size_t must_keep_unused = 2 * size_t(1024 * 1024 * 1024);

  const size_t sys_mem = Common::MemPhysical();
  const size_t keep_unused_mem = std::max(sys_mem / 2, std::min(sys_mem, must_keep_unused));

  m_max_ram_available = sys_mem - keep_unused_mem;

  if (m_max_ram_available == 0)
    ERROR_LOG_FMT(VIDEO, "Not enough system memory for custom resources.");

  m_asset_loader.Initialize();

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

  m_active_assets = {};
  m_pending_assets = {};
  m_asset_handle_to_data.clear();
  m_asset_id_to_handle.clear();
  m_texture_data_asset_cache.clear();
  m_dirty_assets.clear();
  m_ram_used = 0;
}

void CustomResourceManager::MarkAssetDirty(const CustomAssetLibrary::AssetID& asset_id)
{
  std::lock_guard guard(m_dirty_mutex);
  m_dirty_assets.insert(asset_id);
}

CustomResourceManager::TextureTimePair CustomResourceManager::GetTextureDataFromAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  auto& resource = m_texture_data_asset_cache[asset_id];
  if (resource.asset_data != nullptr &&
      resource.asset_data->load_status == AssetData::LoadStatus::ResourceDataAvailable)
  {
    m_active_assets.MakeAssetHighestPriority(resource.asset->GetHandle(), resource.asset);
    return {resource.texture_data, resource.asset->GetLastLoadedTime()};
  }

  // If there is an error, don't try and load again until the error is fixed
  if (resource.asset_data != nullptr && resource.asset_data->has_load_error)
    return {};

  LoadTextureDataAsset(asset_id, std::move(library), &resource);
  m_active_assets.MakeAssetHighestPriority(resource.asset->GetHandle(), resource.asset);

  return {};
}

void CustomResourceManager::LoadTextureDataAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library, InternalTextureDataResource* resource)
{
  if (!resource->asset)
  {
    resource->asset =
        CreateAsset<TextureAsset>(asset_id, AssetData::AssetType::TextureData, std::move(library));
    resource->asset_data = &m_asset_handle_to_data[resource->asset->GetHandle()];
  }

  auto texture_data = resource->asset->GetData();
  if (!texture_data || resource->asset_data->load_status == AssetData::LoadStatus::PendingReload)
  {
    // Tell the system we are still interested in loading this asset
    const auto asset_handle = resource->asset->GetHandle();
    m_pending_assets.MakeAssetHighestPriority(asset_handle,
                                              m_asset_handle_to_data[asset_handle].asset.get());
  }
  else if (resource->asset_data->load_status == AssetData::LoadStatus::LoadFinished)
  {
    resource->texture_data = std::move(texture_data);
    resource->asset_data->load_status = AssetData::LoadStatus::ResourceDataAvailable;
  }
}

void CustomResourceManager::XFBTriggered()
{
  ProcessDirtyAssets();
  ProcessLoadedAssets();

  if (m_ram_used > m_max_ram_available)
  {
    RemoveAssetsUntilBelowMemoryLimit();
  }

  if (m_pending_assets.IsEmpty())
    return;

  if (m_ram_used > m_max_ram_available)
    return;

  const u64 allowed_memory = m_max_ram_available - m_ram_used;
  m_asset_loader.ScheduleAssetsToLoad(m_pending_assets.Elements(), allowed_memory);
}

void CustomResourceManager::ProcessDirtyAssets()
{
  decltype(m_dirty_assets) dirty_assets;

  if (const auto lk = std::unique_lock{m_dirty_mutex, std::try_to_lock})
    std::swap(dirty_assets, m_dirty_assets);

  const auto now = CustomAsset::ClockType::now();
  for (const auto& asset_id : dirty_assets)
  {
    if (const auto it = m_asset_id_to_handle.find(asset_id); it != m_asset_id_to_handle.end())
    {
      const auto asset_handle = it->second;
      AssetData& asset_data = m_asset_handle_to_data[asset_handle];
      asset_data.load_status = AssetData::LoadStatus::PendingReload;
      asset_data.load_request_time = now;

      // Asset was reloaded, clear any errors we might have
      asset_data.has_load_error = false;

      m_pending_assets.InsertAsset(it->second, asset_data.asset.get());

      DEBUG_LOG_FMT(VIDEO, "Dirty asset pending reload: {}", asset_data.asset->GetAssetId());
    }
  }
}

void CustomResourceManager::ProcessLoadedAssets()
{
  const auto asset_handles_loaded = m_asset_loader.TakeLoadedAssetHandles();
  for (const auto& [handle, load_successful] : asset_handles_loaded)
  {
    AssetData& asset_data = m_asset_handle_to_data[handle];

    // If we have a reload request that is newer than our loaded time
    // we need to wait
    if (asset_data.load_request_time > asset_data.asset->GetLastLoadedTime())
      continue;

    m_pending_assets.RemoveAsset(handle);

    asset_data.load_request_time = {};
    if (!load_successful)
    {
      asset_data.has_load_error = true;
    }
    else
    {
      m_active_assets.InsertAsset(handle, asset_data.asset.get());
      asset_data.load_status = AssetData::LoadStatus::LoadFinished;
      m_ram_used += asset_data.asset->GetByteSizeInMemory();
    }
  }
}

void CustomResourceManager::RemoveAssetsUntilBelowMemoryLimit()
{
  const u64 threshold_ram = m_max_ram_available * 8 / 10;
  u64 ram_used = m_ram_used;

  // Clear out least recently used resources until
  // we get safely in our threshold
  while (ram_used > threshold_ram && m_active_assets.Size() > 0)
  {
    auto* const asset = m_active_assets.RemoveLowestPriorityAsset();
    ram_used -= asset->GetByteSizeInMemory();

    AssetData& asset_data = m_asset_handle_to_data[asset->GetHandle()];

    INFO_LOG_FMT(VIDEO, "Unloading asset: {} ({})", asset_data.asset->GetAssetId(),
                 UICommon::FormatSize(asset_data.asset->GetByteSizeInMemory()));

    if (asset_data.type == AssetData::AssetType::TextureData)
    {
      m_texture_data_asset_cache.erase(asset->GetAssetId());
    }
    asset_data.asset->Unload();
    asset_data.load_status = AssetData::LoadStatus::Unloaded;
    asset_data.load_request_time = {};
  }

  // Recalculate to ensure accuracy
  m_ram_used = 0;
  for (auto* const asset : m_active_assets.Elements())
  {
    m_ram_used += asset->GetByteSizeInMemory();
  }
}

}  // namespace VideoCommon
