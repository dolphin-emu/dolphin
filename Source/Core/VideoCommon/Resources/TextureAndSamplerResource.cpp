// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/TextureAndSamplerResource.h"

#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/Resources/TexturePool.h"

namespace VideoCommon
{
TextureAndSamplerResource::TextureAndSamplerResource(Resource::ResourceContext resource_context)
    : Resource(std::move(resource_context))
{
  m_texture_and_sampler_asset = m_resource_context.asset_cache->CreateAsset<TextureAndSamplerAsset>(
      m_resource_context.primary_asset_id, m_resource_context.asset_library, this);
}

void TextureAndSamplerResource::MarkAsActive()
{
  m_resource_context.asset_cache->MarkAssetActive(m_texture_and_sampler_asset);
}

void TextureAndSamplerResource::MarkAsPending()
{
  m_resource_context.asset_cache->MarkAssetPending(m_texture_and_sampler_asset);
}

const std::shared_ptr<TextureAndSamplerResource::Data>& TextureAndSamplerResource::GetData() const
{
  return m_current_data;
}

void TextureAndSamplerResource::ResetData()
{
  m_load_data = std::make_shared<Data>();
}

Resource::TaskComplete TextureAndSamplerResource::CollectPrimaryData()
{
  m_load_data->m_texture_and_sampler_data = m_texture_and_sampler_asset->GetData();
  if (!m_load_data->m_texture_and_sampler_data)
    return Resource::TaskComplete::No;

  auto& texture_data = m_load_data->m_texture_and_sampler_data->texture_data;
  if (texture_data.m_slices.empty())
    return Resource::TaskComplete::Error;

  if (texture_data.m_slices[0].m_levels.empty())
    return Resource::TaskComplete::Error;

  const auto& first_level = texture_data.m_slices[0].m_levels[0];

  auto& config = m_load_data->m_config;
  config.format = first_level.format;
  config.flags = 0;
  config.layers = 1;
  config.levels = 1;
  config.type = m_load_data->m_texture_and_sampler_data->type;
  config.samples = 1;

  config.width = first_level.width;
  config.height = first_level.height;

  return Resource::TaskComplete::Yes;
}

Resource::TaskComplete TextureAndSamplerResource::ProcessData()
{
  auto texture = m_resource_context.texture_pool->AllocateTexture(m_load_data->m_config);
  if (!texture) [[unlikely]]
    return Resource::TaskComplete::Error;

  m_load_data->m_texture = std::move(texture);

  auto& texture_data = m_load_data->m_texture_and_sampler_data->texture_data;
  for (std::size_t slice_index = 0; slice_index < texture_data.m_slices.size(); slice_index++)
  {
    auto& slice = texture_data.m_slices[slice_index];
    for (u32 level_index = 0; level_index < static_cast<u32>(slice.m_levels.size()); ++level_index)
    {
      auto& level = slice.m_levels[level_index];
      m_load_data->m_texture->Load(level_index, level.width, level.height, level.row_length,
                                   level.data.data(), level.data.size(),
                                   static_cast<u32>(slice_index));
    }
  }
  std::swap(m_current_data, m_load_data);

  // Release old data back to the pool
  if (m_load_data)
    m_resource_context.texture_pool->ReleaseTexture(std::move(m_load_data->m_texture));

  return Resource::TaskComplete::Yes;
}

void TextureAndSamplerResource::OnUnloadRequested()
{
  if (!m_current_data)
    return;
  m_resource_context.texture_pool->ReleaseTexture(std::move(m_current_data->m_texture));
  m_current_data = nullptr;
}
}  // namespace VideoCommon
