// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/TextureDataResource.h"

#include "VideoCommon/Assets/CustomAssetCache.h"

namespace VideoCommon
{
TextureDataResource::TextureDataResource(CustomAssetLibrary::AssetID primary_asset_id,
                                         std::shared_ptr<CustomAssetLibrary> asset_library,
                                         CustomAssetCache* asset_cache,
                                         CustomResourceManager* resource_manager,
                                         TexturePool* texture_pool,
                                         Common::AsyncWorkThreadSP* worker_queue)
    : Resource(std::move(primary_asset_id), std::move(asset_library), asset_cache, resource_manager,
               texture_pool, worker_queue)
{
  m_texture_asset =
      m_asset_cache->CreateAsset<TextureAsset>(m_primary_asset_id, m_asset_library, this);
}

std::shared_ptr<CustomTextureData> TextureDataResource::GetData() const
{
  return m_current_texture_data;
}

CustomAsset::TimeType TextureDataResource::GetLoadTime() const
{
  return m_current_time;
}

Resource::TaskComplete TextureDataResource::CollectPrimaryData()
{
  m_load_time = m_texture_asset->GetLastLoadedTime();
  m_load_texture_data = m_texture_asset->GetData();
  if (!m_load_texture_data)
    return Resource::TaskComplete::No;
  return Resource::TaskComplete::Yes;
}

Resource::TaskComplete TextureDataResource::ProcessData()
{
  std::swap(m_current_texture_data, m_load_texture_data);
  m_load_texture_data = nullptr;
  m_current_time = m_load_time;
  return Resource::TaskComplete::Yes;
}

void TextureDataResource::MarkAsActive()
{
  m_asset_cache->MarkAssetActive(m_texture_asset);
}

void TextureDataResource::MarkAsPending()
{
  m_asset_cache->MarkAssetPending(m_texture_asset);
}
}  // namespace VideoCommon
