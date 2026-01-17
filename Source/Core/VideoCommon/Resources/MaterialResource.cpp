// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/MaterialResource.h"

#include <utility>

#include <xxh3.h>

#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/PipelineUtils.h"
#include "VideoCommon/Resources/CustomResourceManager.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
// TODO: absorb this with TextureCacheBase
bool IsAnisotropicEnhancementSafe(const SamplerState::TM0& tm0)
{
  return !(tm0.min_filter == FilterMode::Near && tm0.mag_filter == FilterMode::Near);
}

// TODO: absorb this with TextureCacheBase
SamplerState CalculateSamplerAnisotropy(const SamplerState& initial_sampler)
{
  SamplerState state = initial_sampler;
  if (g_ActiveConfig.iMaxAnisotropy != AnisotropicFilteringMode::Default &&
      IsAnisotropicEnhancementSafe(state.tm0))
  {
    state.tm0.anisotropic_filtering = std::to_underlying(g_ActiveConfig.iMaxAnisotropy);
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
MaterialResource::MaterialResource(Resource::ResourceContext resource_context,
                                   const GXPipelineUid& pipeline_uid)
    : Resource(std::move(resource_context)), m_uid(pipeline_uid)
{
  m_material_asset = m_resource_context.asset_cache->CreateAsset<MaterialAsset>(
      m_resource_context.primary_asset_id, m_resource_context.asset_library, this);
  m_uid_vertex_format_copy =
      g_gfx->CreateNativeVertexFormat(m_uid.vertex_format->GetVertexDeclaration());
  m_uid.vertex_format = m_uid_vertex_format_copy.get();
}

void MaterialResource::ResetData()
{
  if (m_current_data)
  {
    m_current_data->m_shader_resource->RemoveReference(this);
    for (const auto& texture_like_resource : m_current_data->m_texture_like_resources)
    {
      if (texture_like_resource)
        texture_like_resource->RemoveReference(this);
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
    auto* const shader_resource = m_resource_context.resource_manager->GetShaderFromAsset(
        m_load_data->m_material_data->shader_asset, m_load_data->m_shader_key, m_uid,
        m_load_data->m_preprocessor_settings, m_resource_context.asset_library);
    shader_resource->AddReference(this);
    m_load_data->m_shader_resource = shader_resource;
    const auto data_processed = shader_resource->IsDataProcessed();
    if (data_processed == TaskComplete::Error)
      return TaskComplete::Error;

    loaded &= data_processed == TaskComplete::Yes;
  }

  for (std::size_t i = 0; i < m_load_data->m_material_data->textures.size(); i++)
  {
    const auto& texture_and_sampler = m_load_data->m_material_data->textures[i];
    if (texture_and_sampler.asset == "")
      continue;

    const auto texture = m_resource_context.resource_manager->GetTextureAndSamplerFromAsset(
        texture_and_sampler.asset, m_resource_context.asset_library);
    m_load_data->m_texture_like_resources[i] = texture;
    m_load_data->m_texture_like_data[i] = texture->GetData();
    texture->AddReference(this);

    const auto data_processed = texture->IsDataProcessed();
    if (data_processed == TaskComplete::Error)
      return TaskComplete::Error;

    loaded &= data_processed == TaskComplete::Yes;
  }

  if (m_load_data->m_material_data->next_material_asset != "")
  {
    m_load_data->m_next_material = m_resource_context.resource_manager->GetMaterialFromAsset(
        m_load_data->m_material_data->next_material_asset, m_uid, m_resource_context.asset_library);
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
  auto shader_data = m_load_data->m_shader_resource->GetData();

  if (!shader_data) [[unlikely]]
    return Resource::TaskComplete::Error;

  for (std::size_t i = 0; i < m_load_data->m_texture_like_data.size(); i++)
  {
    auto& texture_like_reference = m_load_data->m_texture_like_references[i];
    const auto& texture_and_sampler = m_load_data->m_material_data->textures[i];

    // If the texture doesn't exist, use one of the placeholders
    if (texture_and_sampler.asset == "")
    {
      const auto texture_type = shader_data->GetTextureType(i);
      if (texture_type == AbstractTextureType::Texture_2D)
        texture_like_reference.texture = m_resource_context.invalid_color_texture;
      else if (texture_type == AbstractTextureType::Texture_2DArray)
        texture_like_reference.texture = m_resource_context.invalid_array_texture;
      else if (texture_type == AbstractTextureType::Texture_CubeMap)
        texture_like_reference.texture = m_resource_context.invalid_cubemap_texture;

      if (texture_like_reference.texture == nullptr)
      {
        PanicAlertFmt("Invalid texture (texture_type={}) is not found during material "
                      "resource processing (asset_id={})",
                      texture_type, m_resource_context.primary_asset_id);
      }

      continue;
    }

    auto& texture_like_data = m_load_data->m_texture_like_data[i];

    std::visit(overloaded{[&](const std::shared_ptr<TextureAndSamplerResource::Data>& data) {
                 texture_like_reference.texture = data->GetTexture();
                 texture_like_reference.sampler = CalculateSamplerAnisotropy(data->GetSampler());
                 ;
               }},
               texture_like_data);
  }

  class WorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    WorkItem(std::shared_ptr<MaterialResource::Data> material_resource_data,
             std::shared_ptr<ShaderResource::Data> shader_resource_data,
             VideoCommon::GXPipelineUid* uid, FramebufferState frame_buffer_state)
        : m_material_resource_data(std::move(material_resource_data)),
          m_shader_resource_data(std::move(shader_resource_data)), m_uid(uid),
          m_frame_buffer_state(frame_buffer_state)
    {
    }

    bool Compile() override
    {
      // Sanity check
      if (!m_shader_resource_data->IsCompiled())
      {
        m_material_resource_data->m_processing_finished = true;
        return false;
      }

      AbstractPipelineConfig config;
      config.vertex_shader = m_shader_resource_data->GetVertexShader();
      config.pixel_shader = m_shader_resource_data->GetPixelShader();
      config.geometry_shader = m_shader_resource_data->GetGeometryShader();

      const auto actual_uid = ApplyDriverBugs(*m_uid);

      if (m_material_resource_data->m_material_data->blending_state)
        config.blending_state = *m_material_resource_data->m_material_data->blending_state;
      else
        config.blending_state = actual_uid.blending_state;

      if (m_material_resource_data->m_material_data->depth_state)
        config.depth_state = *m_material_resource_data->m_material_data->depth_state;
      else
        config.depth_state = actual_uid.depth_state;

      config.framebuffer_state = std::move(m_frame_buffer_state);
      config.framebuffer_state.additional_color_attachment_count = 0;

      config.rasterization_state = actual_uid.rasterization_state;
      if (m_material_resource_data->m_material_data->cull_mode)
      {
        config.rasterization_state.cull_mode =
            *m_material_resource_data->m_material_data->cull_mode;
      }

      config.vertex_format = actual_uid.vertex_format;
      config.usage = AbstractPipelineUsage::GX;

      m_material_resource_data->m_pipeline = g_gfx->CreatePipeline(config);

      if (m_material_resource_data->m_pipeline)
      {
        WriteUniforms(m_material_resource_data.get());
      }
      m_material_resource_data->m_processing_finished = true;
      return true;
    }
    void Retrieve() override {}

  private:
    std::shared_ptr<MaterialResource::Data> m_material_resource_data;
    std::shared_ptr<ShaderResource::Data> m_shader_resource_data;
    VideoCommon::GXPipelineUid* m_uid;
    FramebufferState m_frame_buffer_state;
  };

  if (!m_processing_load_data)
  {
    auto wi = m_resource_context.shader_compiler->CreateWorkItem<WorkItem>(
        m_load_data, std::move(shader_data), &m_uid,
        g_framebuffer_manager->GetEFBFramebufferState());

    // We don't need priority, that is already handled by the resource system
    m_resource_context.shader_compiler->QueueWorkItem(std::move(wi), 0);
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

  m_resource_context.asset_cache->MarkAssetActive(m_material_asset);
  for (const auto& texture_like_resource : m_current_data->m_texture_like_resources)
  {
    if (texture_like_resource)
      texture_like_resource->MarkAsActive();
  }
  if (m_current_data->m_shader_resource)
    m_current_data->m_shader_resource->MarkAsActive();
  if (m_current_data->m_next_material)
    m_current_data->m_next_material->MarkAsActive();
}

void MaterialResource::MarkAsPending()
{
  m_resource_context.asset_cache->MarkAssetPending(m_material_asset);
}

void MaterialResource::CreateTextureData(Data* data)
{
  ShaderCode preprocessor_settings;

  const auto& material_data = *data->m_material_data;
  data->m_texture_like_data.clear();
  data->m_texture_like_resources.clear();
  data->m_texture_like_references.clear();
  const u32 custom_sampler_index_offset = 8;
  for (u32 i = 0; i < static_cast<u32>(material_data.textures.size()); i++)
  {
    const auto& texture_and_sampler = material_data.textures[i];
    data->m_texture_like_references.push_back(TextureLikeReference{});

    TextureAndSamplerResource* value = nullptr;
    data->m_texture_like_resources.push_back(value);
    data->m_texture_like_data.push_back(std::shared_ptr<TextureAndSamplerResource::Data>{});

    auto& texture_like_reference = data->m_texture_like_references[i];

    if (texture_and_sampler.asset == "")
    {
      preprocessor_settings.Write("#define HAS_SAMPLER_{} 0\n", i);

      // For an invalid asset, force the sampler to use the default sampler
      texture_like_reference.sampler_origin =
          VideoCommon::TextureSamplerValue::SamplerOrigin::Asset;
      texture_like_reference.texture_hash = "";
    }
    else
    {
      preprocessor_settings.Write("#define HAS_SAMPLER_{} 1\n", i);

      texture_like_reference.sampler_origin = texture_and_sampler.sampler_origin;
      texture_like_reference.texture_hash = texture_and_sampler.texture_hash;
    }

    texture_like_reference.sampler_index = i + custom_sampler_index_offset;
    texture_like_reference.texture = nullptr;
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
  std::size_t max_uniformdata_size = 0;
  for (const auto& property : data->m_material_data->properties)
  {
    max_uniformdata_size += VideoCommon::MaterialProperty::GetMemorySize(property);
  }
  data->m_uniform_data = Common::UniqueBuffer<u8>(max_uniformdata_size);

  // Now write the memory
  u8* uniform_data = data->m_uniform_data.data();
  for (const auto& property : data->m_material_data->properties)
  {
    VideoCommon::MaterialProperty::WriteToMemory(uniform_data, property);
  }
}
}  // namespace VideoCommon
