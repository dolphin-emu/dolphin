// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/CustomResourceManager.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/PipelineUtils.h"
#include "VideoCommon/Resources/InvalidTextures.h"
#include "VideoCommon/VideoEvents.h"

namespace VideoCommon
{
void CustomResourceManager::Initialize()
{
  m_asset_cache.Initialize();
  m_worker_thread.Reset("resource-worker");
  m_host_config.bits = ShaderHostConfig::GetCurrent().bits;
  m_async_shader_compiler = g_gfx->CreateAsyncShaderCompiler();

  m_async_shader_compiler->StartWorkerThreads(1);  // TODO expose to config

  m_xfb_event =
      GetVideoEvents().after_frame_event.Register([this](Core::System&) { XFBTriggered(); });

  m_invalid_array_texture = CreateInvalidArrayTexture();
  m_invalid_color_texture = CreateInvalidColorTexture();
  m_invalid_cubemap_texture = CreateInvalidCubemapTexture();
  m_invalid_transparent_texture = CreateInvalidTransparentTexture();
}

void CustomResourceManager::Shutdown()
{
  if (m_async_shader_compiler)
  {
    m_async_shader_compiler->StopWorkerThreads();
    m_async_shader_compiler->ClearAllWork();
  }

  m_asset_cache.Shutdown();
  m_worker_thread.Shutdown();
  Reset();
}

void CustomResourceManager::Reset()
{
  m_material_resources.clear();
  m_shader_resources.clear();
  m_texture_data_resources.clear();
  m_texture_sampler_resources.clear();

  m_invalid_transparent_texture.reset();
  m_invalid_color_texture.reset();
  m_invalid_cubemap_texture.reset();
  m_invalid_array_texture.reset();

  m_asset_cache.Reset();
  m_texture_pool.Reset();
  m_worker_thread.Reset("resource-worker");
}

void CustomResourceManager::MarkAssetDirty(const CustomAssetLibrary::AssetID& asset_id)
{
  m_asset_cache.MarkAssetDirty(asset_id);
}

void CustomResourceManager::XFBTriggered()
{
  m_asset_cache.Update();
}

void CustomResourceManager::SetHostConfig(const ShaderHostConfig& host_config)
{
  for (auto& [id, shader_resources] : m_shader_resources)
  {
    for (auto& [key, shader_resource] : shader_resources)
    {
      shader_resource->SetHostConfig(host_config);

      // Hack to get access to resource internals
      Resource* resource = shader_resource.get();

      // Tell shader and references to trigger a reload
      // on next usage
      resource->NotifyAssetChanged(false);
    }
  }

  m_host_config.bits = host_config.bits;
}

TextureDataResource* CustomResourceManager::GetTextureDataFromAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  auto& resource = m_texture_data_resources[asset_id];
  if (resource == nullptr)
  {
    resource =
        std::make_unique<TextureDataResource>(CreateResourceContext(asset_id, std::move(library)));
  }
  resource->Process();
  return resource.get();
}

MaterialResource* CustomResourceManager::GetMaterialFromAsset(
    const CustomAssetLibrary::AssetID& asset_id, const GXPipelineUid& pipeline_uid,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  auto& resource = m_material_resources[asset_id][PipelineToHash(pipeline_uid)];
  if (resource == nullptr)
  {
    resource = std::make_unique<MaterialResource>(
        CreateResourceContext(asset_id, std::move(library)), pipeline_uid);
  }
  resource->Process();
  return resource.get();
}

ShaderResource*
CustomResourceManager::GetShaderFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                                          std::size_t shader_key, const GXPipelineUid& pipeline_uid,
                                          const std::string& preprocessor_settings,
                                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  auto& resource = m_shader_resources[asset_id][shader_key];
  if (resource == nullptr)
  {
    resource = std::make_unique<ShaderResource>(CreateResourceContext(asset_id, std::move(library)),
                                                pipeline_uid, preprocessor_settings, m_host_config);
  }
  resource->Process();
  return resource.get();
}

TextureAndSamplerResource* CustomResourceManager::GetTextureAndSamplerFromAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  auto& resource = m_texture_sampler_resources[asset_id];
  if (resource == nullptr)
  {
    resource = std::make_unique<TextureAndSamplerResource>(
        CreateResourceContext(asset_id, std::move(library)));
  }
  resource->Process();
  return resource.get();
}

Resource::ResourceContext CustomResourceManager::CreateResourceContext(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  return Resource::ResourceContext{asset_id,
                                   std::move(library),
                                   &m_asset_cache,
                                   this,
                                   &m_texture_pool,
                                   &m_worker_thread,
                                   m_async_shader_compiler.get(),
                                   m_invalid_array_texture.get(),
                                   m_invalid_color_texture.get(),
                                   m_invalid_cubemap_texture.get(),
                                   m_invalid_transparent_texture.get()};
}
}  // namespace VideoCommon
