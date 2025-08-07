// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/MaterialResource.h"

#include <xxh3.h>

#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/Resources/CustomResourceManager.h"
#include "VideoCommon/ShaderCacheUtils.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
// TODO: absorb this with TextureCacheBase
bool IsAnisostropicEnhancementSafe(const SamplerState::TM0& tm0)
{
  return !(tm0.min_filter == FilterMode::Near && tm0.mag_filter == FilterMode::Near);
}

// TODO: absorb this with TextureCacheBase
SamplerState CalculateSamplerAnsiotropy(const SamplerState& initial_sampler)
{
  SamplerState state = initial_sampler;
  if (g_ActiveConfig.iMaxAnisotropy != AnisotropicFilteringMode::Default &&
      IsAnisostropicEnhancementSafe(state.tm0))
  {
    state.tm0.anisotropic_filtering = Common::ToUnderlying(g_ActiveConfig.iMaxAnisotropy);
  }

  if (state.tm0.anisotropic_filtering != 0)
  {
    // https://www.opengl.org/registry/specs/EXT/texture_filter_anisotropic.txt
    // For predictable results on all hardware/drivers, only use one of:
    //	GL_LINEAR + GL_LINEAR (No Mipmaps [Bilinear])
    //	GL_LINEAR + GL_LINEAR_MIPMAP_LINEAR (w/ Mipmaps [Trilinear])
    // Letting the game set other combinations will have varying arbitrary results;
    // possibly being interpreted as equal to bilinear/trilinear, implicitly
    // disabling anisotropy, or changing the anisotropic algorithm employed.
    state.tm0.min_filter = FilterMode::Linear;
    state.tm0.mag_filter = FilterMode::Linear;
    state.tm0.mipmap_filter = FilterMode::Linear;
  }
  return state;
}
}  // namespace

namespace VideoCommon
{
MaterialResource::MaterialResource(CustomAssetLibrary::AssetID primary_asset_id,
                                   std::shared_ptr<CustomAssetLibrary> asset_library,
                                   CustomAssetCache* asset_cache,
                                   CustomResourceManager* resource_manager,
                                   TexturePool* texture_pool,
                                   Common::AsyncWorkThreadSP* worker_queue,
                                   const GXPipelineUid& pipeline_uid)
    : Resource(std::move(primary_asset_id), std::move(asset_library), asset_cache, resource_manager,
               texture_pool, worker_queue),
      m_uid(pipeline_uid)
{
  m_material_asset =
      m_asset_cache->CreateAsset<RasterMaterialAsset>(m_primary_asset_id, m_asset_library, this);
}

void MaterialResource::ResetData()
{
  if (m_current_data)
  {
    m_current_data->m_shader_resource->RemoveReference(this);
    for (const auto& texture_resource : m_current_data->m_texture_like_resources)
    {
      std::visit(
          overloaded{
              [this](TextureAndSamplerResource* resource) { resource->RemoveReference(this); },
              [this](RenderTargetResource* resource) { resource->RemoveReference(this); }},
          texture_resource);
    }
    if (m_current_data->m_next_material)
      m_current_data->m_next_material->RemoveReference(this);
  }
  m_load_data = std::make_shared<Data>();
  m_processing_load_data = false;
}

Resource::TaskComplete MaterialResource::CollectPrimaryData()
{
  const auto material_data = m_material_asset->GetData();
  if (!material_data) [[unlikely]]
  {
    return Resource::TaskComplete::No;
  }
  m_load_data->m_material_data = material_data;

  // A shader asset is required to function
  if (m_load_data->m_material_data->shader_asset == "")
  {
    return Resource::TaskComplete::Error;
  }

  CreateTextureData(m_load_data.get());
  SetShaderKey(m_load_data.get(), &m_uid);

  return Resource::TaskComplete::Yes;
}

Resource::TaskComplete MaterialResource::CollectDependencyData()
{
  bool loaded = true;
  {
    auto* const shader_resource = m_resource_manager->GetShaderFromAsset(
        m_load_data->m_material_data->shader_asset, m_load_data->m_shader_key, m_uid,
        m_load_data->m_preprocessor_settings, m_asset_library);
    shader_resource->AddReference(this);
    m_load_data->m_shader_resource = shader_resource;
    const auto data_processed = shader_resource->IsDataProcessed();
    if (data_processed == TaskComplete::Error)
      return TaskComplete::Error;

    loaded &= data_processed == TaskComplete::Yes;
  }

  for (std::size_t i = 0; i < m_load_data->m_material_data->pixel_textures.size(); i++)
  {
    const auto& texture_and_sampler = m_load_data->m_material_data->pixel_textures[i];
    if (texture_and_sampler.asset == "")
      continue;

    if (texture_and_sampler.is_render_target)
    {
      const auto render_target =
          m_resource_manager->GetRenderTargetFromAsset(texture_and_sampler.asset, m_asset_library);
      m_load_data->m_texture_like_resources[i] = render_target;
      m_load_data->m_texture_like_data[i] = render_target->GetData();
      render_target->AddReference(this);

      const auto data_processed = render_target->IsDataProcessed();
      if (data_processed == TaskComplete::Error)
        return TaskComplete::Error;

      loaded &= data_processed == TaskComplete::Yes;
    }
    else
    {
      const auto texture = m_resource_manager->GetTextureAndSamplerFromAsset(
          texture_and_sampler.asset, m_asset_library);
      m_load_data->m_texture_like_resources[i] = texture;
      m_load_data->m_texture_like_data[i] = texture->GetData();
      texture->AddReference(this);

      const auto data_processed = texture->IsDataProcessed();
      if (data_processed == TaskComplete::Error)
        return TaskComplete::Error;

      loaded &= data_processed == TaskComplete::Yes;
    }
  }

  if (m_load_data->m_material_data->next_material_asset != "")
  {
    m_load_data->m_next_material = m_resource_manager->GetMaterialFromAsset(
        m_load_data->m_material_data->next_material_asset, m_uid, m_asset_library);
    m_load_data->m_next_material->AddReference(this);
    const auto data_processed = m_load_data->m_next_material->IsDataProcessed();
    if (data_processed == TaskComplete::Error)
      return TaskComplete::Error;

    loaded &= data_processed == TaskComplete::Yes;
  }

  return loaded ? TaskComplete::Yes : TaskComplete::No;
}

Resource::TaskComplete MaterialResource::ProcessData()
{
  for (std::size_t i = 0; i < m_load_data->m_texture_like_data.size(); i++)
  {
    auto& texture_like_data = m_load_data->m_texture_like_data[i];
    auto& texture_like_reference = m_load_data->m_texture_like_references[i];

    if (!texture_like_reference.texture)
    {
      std::visit(overloaded{[&](const std::shared_ptr<TextureAndSamplerResource::Data>& data) {
                              texture_like_reference.texture = data->GetTexture();
                              texture_like_reference.sampler =
                                  CalculateSamplerAnsiotropy(data->GetSampler());
                              ;
                            },
                            [&](const std::shared_ptr<RenderTargetResource::Data>& data) {
                              texture_like_reference.texture = data->GetTexture();
                              texture_like_reference.sampler =
                                  CalculateSamplerAnsiotropy(data->GetSampler());
                            }},
                 texture_like_data);
    }
  }

  if (!m_processing_load_data)
  {
    auto work =
        [load_data = m_load_data, shader_resource_data = m_load_data->m_shader_resource->GetData(),
         efb_frame_buffer_state = g_framebuffer_manager->GetEFBFramebufferState(), uid = &m_uid]() {
          // Sanity check
          if (!shader_resource_data->IsCompiled())
          {
            load_data->m_processing_finished = true;
            return;
          }

          AbstractPipelineConfig config;
          config.vertex_shader = shader_resource_data->GetVertexShader();
          config.pixel_shader = shader_resource_data->GetPixelShader();
          config.geometry_shader = shader_resource_data->GetGeometryShader();

          const auto actual_uid = ApplyDriverBugs(*uid);

          if (load_data->m_material_data->blending_state)
            config.blending_state = *load_data->m_material_data->blending_state;
          else
            config.blending_state = actual_uid.blending_state;

          if (load_data->m_material_data->depth_state)
            config.depth_state = *load_data->m_material_data->depth_state;
          else
            config.depth_state = actual_uid.depth_state;

          config.framebuffer_state = std::move(efb_frame_buffer_state);
          config.framebuffer_state.additional_color_attachment_count = 0;

          config.rasterization_state = actual_uid.rasterization_state;
          if (load_data->m_material_data->cull_mode)
            config.rasterization_state.cullmode = *load_data->m_material_data->cull_mode;

          config.vertex_format = actual_uid.vertex_format;
          config.usage = AbstractPipelineUsage::GX;

          load_data->m_pipeline = g_gfx->CreatePipeline(config);

          if (load_data->m_pipeline)
          {
            WriteUniforms(load_data.get());
          }
          load_data->m_processing_finished = true;
        };
    m_worker_queue->Push(std::move(work));
    m_processing_load_data = true;
  }

  if (!m_load_data->m_processing_finished)
    return TaskComplete::No;

  if (!m_load_data->m_pipeline)
  {
    return TaskComplete::Error;
  }

  std::swap(m_current_data, m_load_data);
  return TaskComplete::Yes;
}

void MaterialResource::MarkAsActive()
{
  if (!m_current_data) [[unlikely]]
    return;

  m_asset_cache->MarkAssetActive(m_material_asset);
  for (const auto& texture_resource : m_current_data->m_texture_like_resources)
  {
    std::visit(overloaded{[](TextureAndSamplerResource* resource) { resource->MarkAsActive(); },
                          [](RenderTargetResource* resource) { resource->MarkAsActive(); }},
               texture_resource);
  }
  if (m_current_data->m_shader_resource)
    m_current_data->m_shader_resource->MarkAsActive();
  if (m_current_data->m_next_material)
    m_current_data->m_next_material->MarkAsActive();
}

void MaterialResource::MarkAsPending()
{
  m_asset_cache->MarkAssetPending(m_material_asset);
}

void MaterialResource::CreateTextureData(Data* data)
{
  ShaderCode preprocessor_settings;

  const auto& material_data = *data->m_material_data;
  data->m_texture_like_data.clear();
  data->m_texture_like_resources.clear();
  data->m_texture_like_references.clear();
  const u32 custom_sampler_index_offset = 8;
  for (u32 i = 0; i < static_cast<u32>(material_data.pixel_textures.size()); i++)
  {
    const auto& texture_and_sampler = material_data.pixel_textures[i];
    data->m_texture_like_references.push_back(TextureLikeReference{});
    if (texture_and_sampler.is_render_target)
    {
      RenderTargetResource* value = nullptr;
      data->m_texture_like_resources.push_back(value);
      data->m_texture_like_data.push_back(std::shared_ptr<RenderTargetResource::Data>{});
    }
    else
    {
      TextureAndSamplerResource* value = nullptr;
      data->m_texture_like_resources.push_back(value);
      data->m_texture_like_data.push_back(std::shared_ptr<TextureAndSamplerResource::Data>{});
    }

    if (texture_and_sampler.asset == "")
    {
      preprocessor_settings.Write("#define HAS_SAMPLER_{} 0\n", i);
    }
    else
    {
      auto& texture_like_reference = data->m_texture_like_references[i];

      texture_like_reference.sampler_origin = texture_and_sampler.sampler_origin;
      texture_like_reference.sampler_index = i + custom_sampler_index_offset;
      texture_like_reference.texture_hash = texture_and_sampler.texture_hash;
      texture_like_reference.texture = nullptr;

      preprocessor_settings.Write("#define HAS_SAMPLER_{} 1\n", i);
    }
  }

  data->m_preprocessor_settings = preprocessor_settings.GetBuffer();
}

void MaterialResource::SetShaderKey(Data* data, GXPipelineUid* uid)
{
  XXH3_state_t shader_key_hash;
  XXH3_INITSTATE(&shader_key_hash);
  XXH3_64bits_reset_withSeed(&shader_key_hash, static_cast<XXH64_hash_t>(1));

  UpdateHashWithPipeline(*uid, &shader_key_hash);
  XXH3_64bits_update(&shader_key_hash, data->m_preprocessor_settings.c_str(),
                     data->m_preprocessor_settings.size());

  data->m_shader_key = XXH3_64bits_digest(&shader_key_hash);
}

void MaterialResource::WriteUniforms(Data* data)
{
  // Calculate the size in memory of the buffer
  std::size_t max_pixeldata_size = 0;
  for (const auto& property : data->m_material_data->pixel_properties)
  {
    max_pixeldata_size += VideoCommon::MaterialProperty2::GetMemorySize(property);
  }
  data->m_pixel_uniform_data.resize(max_pixeldata_size);

  // Now write the memory
  u8* pixel_data = data->m_pixel_uniform_data.data();
  for (const auto& property : data->m_material_data->pixel_properties)
  {
    VideoCommon::MaterialProperty2::WriteToMemory(pixel_data, property);
  }

  // Calculate the size in memory of the buffer
  std::size_t max_vertexdata_size = 0;
  for (const auto& property : data->m_material_data->vertex_properties)
  {
    max_vertexdata_size += VideoCommon::MaterialProperty2::GetMemorySize(property);
  }
  data->m_vertex_uniform_data.resize(max_vertexdata_size);

  // Now write the memory
  u8* vertex_data = data->m_vertex_uniform_data.data();
  for (const auto& property : data->m_material_data->vertex_properties)
  {
    VideoCommon::MaterialProperty2::WriteToMemory(vertex_data, property);
  }
}
}  // namespace VideoCommon
