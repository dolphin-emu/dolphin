// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/RenderTargetResource.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/Resources/TexturePool.h"

namespace VideoCommon
{
RenderTargetResource::RenderTargetResource(Resource::ResourceContext resource_context)
    : Resource(std::move(resource_context))
{
  m_render_target_asset = m_resource_context.asset_cache->CreateAsset<RenderTargetAsset>(
      m_resource_context.primary_asset_id, m_resource_context.asset_library, this);
}

void RenderTargetResource::MarkAsActive()
{
  if (!m_current_data)
    return;

  m_resource_context.asset_cache->MarkAssetActive(m_render_target_asset);
}

void RenderTargetResource::MarkAsPending()
{
  m_resource_context.asset_cache->MarkAssetPending(m_render_target_asset);
}

AbstractTexture* RenderTargetResource::Data::GetTexture() const
{
  return m_texture.get();
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
  if (m_load_data->m_render_target_data->render_type == RenderTargetData::RenderType::ScreenColor)
  {
    config.format = g_framebuffer_manager->GetEFBColorFormat();
  }
  else if (m_load_data->m_render_target_data->render_type ==
           RenderTargetData::RenderType::ScreenDepth)
  {
    config.format = g_framebuffer_manager->GetEFBDepthFormat();
  }
  else
  {
    config.format = m_load_data->m_render_target_data->texture_format;
  }
  config.flags = AbstractTextureFlag::AbstractTextureFlag_RenderTarget;
  config.layers = 1;
  config.levels = 1;

  // TODO: We force to a 2d array texture for now, need to figure out
  // how to handle cubemaps.  We use a 2d array texture
  // primarily because we render this with imgui
  // and it currently only supports array types
  config.type = AbstractTextureType::Texture_2DArray;
  config.samples = g_framebuffer_manager->GetEFBSamples();

  config.width = m_load_data->m_render_target_data->width;
  config.height = m_load_data->m_render_target_data->height;

  return Resource::TaskComplete::Yes;
}

Resource::TaskComplete RenderTargetResource::ProcessData()
{
  if (auto texture = m_resource_context.texture_pool->AllocateTexture(m_load_data->m_config))
  {
    m_load_data->m_texture = std::move(texture);
    std::swap(m_current_data, m_load_data);

    // Release old data back to the pool
    if (m_load_data)
    {
      m_resource_context.texture_pool->ReleaseTexture(std::move(m_load_data->m_texture));
    }

    return Resource::TaskComplete::Yes;
  }

  return Resource::TaskComplete::Error;
}

void RenderTargetResource::OnUnloadRequested()
{
  if (!m_current_data)
    return;
  m_resource_context.texture_pool->ReleaseTexture(std::move(m_current_data->m_texture));
  m_current_data = nullptr;
}
}  // namespace VideoCommon
