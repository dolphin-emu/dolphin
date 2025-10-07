// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/TextureDataResource.h"

#include "VideoCommon/Assets/CustomAssetCache.h"

namespace VideoCommon
{
TextureDataResource::TextureDataResource(Resource::ResourceContext resource_context)
    : Resource(std::move(resource_context))
{
  m_texture_asset = m_resource_context.asset_cache->CreateAsset<TextureAsset>(
      m_resource_context.primary_asset_id, m_resource_context.asset_library, this);
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
  m_resource_context.asset_cache->MarkAssetActive(m_texture_asset);
}

void TextureDataResource::MarkAsPending()
{
  m_resource_context.asset_cache->MarkAssetPending(m_texture_asset);
}
}  // namespace VideoCommon
