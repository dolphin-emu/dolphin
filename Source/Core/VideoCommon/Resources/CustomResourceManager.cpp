// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/CustomResourceManager.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Resources/InvalidTextures.h"
#include "VideoCommon/ShaderCacheUtils.h"
#include "VideoCommon/VideoEvents.h"

namespace VideoCommon
{
void CustomResourceManager::Initialize()
{
  m_asset_cache.Initialize();
  m_worker_thread.Reset("resource-worker");
  m_host_config.bits = ShaderHostConfig::GetCurrent().bits;
  m_async_shader_compiler = g_gfx->CreateAsyncShaderCompiler();

  m_async_shader_compiler->StartWorkerThreads(1);  // TODO

  m_xfb_event =
      AfterFrameEvent::Register([this](Core::System&) { XFBTriggered(); }, "CustomResourceManager");

  m_invalid_array_texture = CreateInvalidArrayTexture();
  m_invalid_color_texture = CreateInvalidColorTexture();
  m_invalid_cubemap_texture = CreateInvalidCubemapTexture();
  m_invalid_transparent_texture = CreateInvalidTransparentTexture();
}

void CustomResourceManager::Shutdown()
{
  if (m_async_shader_compiler)
    m_async_shader_compiler->StopWorkerThreads();

  m_asset_cache.Shutdown();
  m_worker_thread.Shutdown();
  Reset();
}

void CustomResourceManager::Reset()
{
  m_material_resources.clear();
  m_mesh_resources.clear();
  m_render_target_resources.clear();
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

  /*for (auto& [id, texture_resource] : m_texture_sampler_resources)
  {
    // Hack to get access to resource internals
    Resource* resource = &texture_resource;

    // Unload our GPU data and
    // tell texture and references to trigger a reload
    // on next usage
    resource->NotifyAssetUnloaded();
    resource->NotifyAssetChanged(false);
  }

  for (auto& [id, shader_resource] : m_shader_resources)
  {
    // Hack to get access to resource internals
    Resource* resource = &shader_resource;

    // Unload our GPU data and
    // tell shader and references to trigger a reload
    // on next usage
    resource->NotifyAssetUnloaded();
    resource->NotifyAssetChanged(false);
  }*/
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
  const auto [it, added] = m_texture_data_resources.try_emplace(asset_id, nullptr);
  if (added)
  {
    it->second = std::make_unique<TextureDataResource>(CreateResourceContext(asset_id, library));
  }
  ProcessResource(it->second.get());
  return it->second.get();
}

MaterialResource* CustomResourceManager::GetMaterialFromAsset(
    const CustomAssetLibrary::AssetID& asset_id, const GXPipelineUid& pipeline_uid,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto [it_container, added_container] =
      m_material_resources.try_emplace(asset_id, PipelineIdToMaterial{});
  const auto [it_uid, added_uid] =
      it_container->second.try_emplace(PipelineToHash(pipeline_uid), nullptr);
  if (added_uid)
  {
    it_uid->second =
        std::make_unique<MaterialResource>(CreateResourceContext(asset_id, library), pipeline_uid);
  }
  ProcessResource(it_uid->second.get());
  return it_uid->second.get();
}

ShaderResource*
CustomResourceManager::GetShaderFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                                          std::size_t shader_key, const GXPipelineUid& pipeline_uid,
                                          const std::string& preprocessor_settings,
                                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto [it_container, added_container] =
      m_shader_resources.try_emplace(asset_id, ShaderKeyToShader{});
  const auto [it_key, added_key] = it_container->second.try_emplace(shader_key, nullptr);
  if (added_key)
  {
    it_key->second = std::make_unique<ShaderResource>(CreateResourceContext(asset_id, library),
                                                      pipeline_uid, preprocessor_settings);
    it_key->second->SetHostConfig(m_host_config);
  }
  ProcessResource(it_key->second.get());
  return it_key->second.get();
}

TextureAndSamplerResource* CustomResourceManager::GetTextureAndSamplerFromAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto [it, added] = m_texture_sampler_resources.try_emplace(asset_id, nullptr);
  if (added)
  {
    it->second =
        std::make_unique<TextureAndSamplerResource>(CreateResourceContext(asset_id, library));
  }
  ProcessResource(it->second.get());
  return it->second.get();
}

RenderTargetResource* CustomResourceManager::GetRenderTargetFromAsset(
    const CustomAssetLibrary::AssetID& asset_id,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto [it, added] = m_render_target_resources.try_emplace(asset_id, nullptr);
  if (added)
  {
    it->second = std::make_unique<RenderTargetResource>(CreateResourceContext(asset_id, library));
  }
  ProcessResource(it->second.get());
  return it->second.get();
}

MeshResource*
CustomResourceManager::GetMeshFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                                        const GXPipelineUid& pipeline_uid,
                                        std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  const auto [it_container, added_container] =
      m_mesh_resources.try_emplace(asset_id, PipelineIdToMesh{});
  const auto [it_uid, added_uid] =
      it_container->second.try_emplace(PipelineToHash(pipeline_uid), nullptr);
  if (added_uid)
  {
    it_uid->second =
        std::make_unique<MeshResource>(CreateResourceContext(asset_id, library), pipeline_uid);
  }
  ProcessResource(it_uid->second.get());
  return it_uid->second.get();
}

Resource::ResourceContext CustomResourceManager::CreateResourceContext(
    const CustomAssetLibrary::AssetID& asset_id,
    const std::shared_ptr<VideoCommon::CustomAssetLibrary>& library)
{
  return Resource::ResourceContext{asset_id,
                                   library,
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

void CustomResourceManager::ProcessResource(Resource* resource)
{
  resource->MarkAsActive();

  const auto data_processed = resource->IsDataProcessed();
  if (data_processed == Resource::TaskComplete::Yes ||
      data_processed == Resource::TaskComplete::Error)
  {
    resource->MarkAsActive();
    if (data_processed == Resource::TaskComplete::Error)
      return;
  }

  // Early out if we're already at our end state
  if (resource->GetState() == Resource::State::DataAvailable)
    return;

  ProcessResourceState(resource);
}

void CustomResourceManager::ProcessResourceState(Resource* resource)
{
  Resource::State next_state = resource->GetState();
  Resource::TaskComplete task_complete = Resource::TaskComplete::No;
  switch (resource->GetState())
  {
  case Resource::State::ReloadData:
    resource->ResetData();
    task_complete = Resource::TaskComplete::Yes;
    next_state = Resource::State::CollectingPrimaryData;
    break;
  case Resource::State::CollectingPrimaryData:
    task_complete = resource->CollectPrimaryData();
    next_state = Resource::State::CollectingDependencyData;
    if (task_complete == Resource::TaskComplete::No)
      resource->MarkAsPending();
    break;
  case Resource::State::CollectingDependencyData:
    task_complete = resource->CollectDependencyData();
    next_state = Resource::State::ProcessingData;
    break;
  case Resource::State::ProcessingData:
    task_complete = resource->ProcessData();
    next_state = Resource::State::DataAvailable;
  };

  if (task_complete == Resource::TaskComplete::Yes)
  {
    resource->m_state = next_state;
    if (next_state == Resource::State::DataAvailable)
    {
      resource->m_data_processed = task_complete;
    }
    else
    {
      ProcessResourceState(resource);
    }
  }
  else if (task_complete == Resource::TaskComplete::Error)
  {
    resource->m_data_processed = task_complete;
  }
}
}  // namespace VideoCommon
