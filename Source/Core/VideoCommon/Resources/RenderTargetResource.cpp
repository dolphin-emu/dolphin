// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/RenderTargetResource.h"

#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/Resources/TexturePool.h"

namespace VideoCommon
{
RenderTargetResource::RenderTargetResource(CustomAssetLibrary::AssetID primary_asset_id,
                                           std::shared_ptr<CustomAssetLibrary> asset_library,
                                           CustomAssetCache* asset_cache,
                                           CustomResourceManager* resource_manager,
                                           TexturePool* texture_pool,
                                           Common::AsyncWorkThreadSP* worker_queue)
    : Resource(std::move(primary_asset_id), std::move(asset_library), asset_cache, resource_manager,
               texture_pool, worker_queue)
{
  m_render_target_asset =
      m_asset_cache->CreateAsset<RenderTargetAsset>(m_primary_asset_id, m_asset_library, this);
}

void RenderTargetResource::MarkAsActive()
{
  if (!m_current_data)
    return;

  m_asset_cache->MarkAssetActive(m_render_target_asset);
}

void RenderTargetResource::MarkAsPending()
{
  m_asset_cache->MarkAssetPending(m_render_target_asset);
}

const std::shared_ptr<RenderTargetResource::Data>& RenderTargetResource::GetData() const
{
  return m_current_data;
}

void RenderTargetResource::ResetData()
{
  m_load_data = std::make_shared<Data>();
}

Resource::TaskComplete RenderTargetResource::CollectPrimaryData()
{
  m_load_data->m_render_target_data = m_render_target_asset->GetData();
  m_load_data->m_load_time = m_render_target_asset->GetLastLoadedTime();
  if (!m_load_data->m_render_target_data)
    return Resource::TaskComplete::No;

  auto& config = m_load_data->m_config;
  config.format = m_load_data->m_render_target_data->texture_format;
  config.flags = AbstractTextureFlag::AbstractTextureFlag_RenderTarget;
  config.layers = 1;
  config.levels = 1;
  config.type = m_load_data->m_render_target_data->type;
  config.samples = 1;

  config.width = m_load_data->m_render_target_data->width;
  config.height = m_load_data->m_render_target_data->height;

  return Resource::TaskComplete::Yes;
}

Resource::TaskComplete RenderTargetResource::ProcessData()
{
  if (auto texture = m_texture_pool->AllocateTexture(m_load_data->m_config))
  {
    m_load_data->m_texture = std::move(*texture);
    std::swap(m_current_data, m_load_data);

    // Release old data back to the pool
    if (m_load_data)
      m_texture_pool->ReleaseTexture(std::move(m_load_data->m_texture));

    return Resource::TaskComplete::Yes;
  }

  return Resource::TaskComplete::Error;
}

void RenderTargetResource::OnUnloadRequested()
{
  if (!m_current_data)
    return;
  m_texture_pool->ReleaseTexture(std::move(m_current_data->m_texture));
  m_current_data = nullptr;
}
}  // namespace VideoCommon
