// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/ShaderResource.h"

#include <string_view>

#include <fmt/format.h>

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/ShaderCacheUtils.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
namespace
{
std::unique_ptr<AbstractShader>
CompileGeometryShader(const GeometryShaderUid& uid, APIType api_type, ShaderHostConfig host_config)
{
  const ShaderCode source_code =
      GenerateGeometryShaderCode(api_type, host_config, uid.GetUidData());
  return g_gfx->CreateShaderFromSource(ShaderStage::Geometry, source_code.GetBuffer(),
                                       fmt::format("Geometry shader: {}", *uid.GetUidData()));
}

std::unique_ptr<AbstractShader> CompilePixelShader(const PixelShaderUid& uid,
                                                   std::string_view preprocessor_settings,
                                                   APIType api_type,
                                                   const ShaderHostConfig& host_config,
                                                   const RasterShaderData& shader_data)
{
  ShaderCode shader_code;

  // Write any preprocessor values that were passed in
  shader_code.Write("{}", preprocessor_settings);

  // TODO: in the future we could dynamically determine the amount of samplers
  // available, for now just hardcode to start at 8 (the first non game
  // sampler index available)
  const std::size_t custom_sampler_index_offset = 8;
  for (std::size_t i = 0; i < shader_data.m_pixel_samplers.size(); i++)
  {
    const auto& pixel_sampler = shader_data.m_pixel_samplers[i];
    std::string_view sampler_type = "";
    switch (pixel_sampler.type)
    {
    case AbstractTextureType::Texture_2D:
      sampler_type = "sampler2D";
      break;
    case AbstractTextureType::Texture_2DArray:
      sampler_type = "sampler2DArray";
      break;
    case AbstractTextureType::Texture_CubeMap:
      sampler_type = "samplerCube";
      break;
    };
    shader_code.Write("SAMPLER_BINDING({}) uniform {} samp_{};\n", custom_sampler_index_offset + i,
                      sampler_type, pixel_sampler.name);

    // Sampler usage is passed in from the material
    // Write a new preprocessor value with the sampler name
    // for easier code in the shader
    shader_code.Write("#ifdef HAS_SAMPLER_{}\n", i);
    shader_code.Write("#define HAS_{} 1\n", pixel_sampler.name);
    shader_code.Write("#else\n");
    shader_code.Write("#define HAS_{} 0\n", pixel_sampler.name);
    shader_code.Write("#endif\n");

    shader_code.Write("\n");
  }
  shader_code.Write("\n");

  // Now write the custom shader
  shader_code.Write("{}", ReplaceAll(shader_data.m_pixel_source, "\r\n", "\n"));

  // Write out the uniform data
  ShaderCode uniform_code;
  for (const auto& [name, property] : shader_data.m_pixel_properties)
  {
    VideoCommon::ShaderProperty2::WriteAsShaderCode(uniform_code, name, property);
  }
  if (!shader_data.m_pixel_properties.empty())
    uniform_code.Write("\n\n");

  // Compile the shader
  CustomPixelContents contents{.shader = shader_code.GetBuffer(),
                               .uniforms = uniform_code.GetBuffer()};
  const ShaderCode source_code =
      GeneratePixelShaderCode(api_type, host_config, uid.GetUidData(), contents);
  return g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer(),
                                       "Custom Pixel Shader");
}

std::unique_ptr<AbstractShader> CompileVertexShader(const VertexShaderUid& uid,
                                                    std::string_view preprocessor_settings,
                                                    APIType api_type,
                                                    const ShaderHostConfig& host_config,
                                                    const RasterShaderData& shader_data)
{
  ShaderCode shader_code;

  // Write any preprocessor values that were passed in
  shader_code.Write("{}", preprocessor_settings);

  // Now write the custom shader
  shader_code.Write("{}", ReplaceAll(shader_data.m_vertex_source, "\r\n", "\n"));

  // Write out the uniform data
  ShaderCode uniform_code;
  for (const auto& [name, property] : shader_data.m_vertex_properties)
  {
    VideoCommon::ShaderProperty2::WriteAsShaderCode(uniform_code, name, property);
  }
  if (!shader_data.m_vertex_properties.empty())
    uniform_code.Write("\n\n");

  // Compile the shader
  CustomVertexContents contents{.shader = shader_code.GetBuffer(),
                                .uniforms = uniform_code.GetBuffer()};
  const ShaderCode source_code =
      GenerateVertexShaderCode(api_type, host_config, uid.GetUidData(), contents);
  return g_gfx->CreateShaderFromSource(ShaderStage::Vertex, source_code.GetBuffer(),
                                       "Custom Vertex Shader");
}
}  // namespace
ShaderResource::ShaderResource(CustomAssetLibrary::AssetID primary_asset_id,
                               std::shared_ptr<CustomAssetLibrary> asset_library,
                               CustomAssetCache* asset_cache,
                               CustomResourceManager* resource_manager, TexturePool* texture_pool,
                               Common::AsyncWorkThreadSP* worker_queue,
                               const GXPipelineUid& pipeline_uid,
                               const std::string& preprocessor_setting)
    : Resource(std::move(primary_asset_id), std::move(asset_library), asset_cache, resource_manager,
               texture_pool, worker_queue),
      m_uid(pipeline_uid), m_preprocessor_settings(preprocessor_setting)
{
  m_shader_asset =
      m_asset_cache->CreateAsset<RasterShaderAsset>(m_primary_asset_id, m_asset_library, this);
}

void ShaderResource::SetHostConfig(const ShaderHostConfig& host_config)
{
  m_shader_host_config.bits = host_config.bits;
}

void ShaderResource::MarkAsPending()
{
  m_asset_cache->MarkAssetPending(m_shader_asset);
}

void ShaderResource::MarkAsActive()
{
  m_asset_cache->MarkAssetActive(m_shader_asset);
}

AbstractShader* ShaderResource::Data::GetVertexShader() const
{
  if (!m_vertex_shader)
    return nullptr;
  return m_vertex_shader.get();
}

AbstractShader* ShaderResource::Data::GetPixelShader() const
{
  if (!m_pixel_shader)
    return nullptr;
  return m_pixel_shader.get();
}

AbstractShader* ShaderResource::Data::GetGeometryShader() const
{
  if (!m_geometry_shader)
    return nullptr;
  return m_geometry_shader.get();
}

bool ShaderResource::Data::IsCompiled() const
{
  return m_vertex_shader && m_pixel_shader && (!m_needs_geometry_shader || m_geometry_shader);
}

void ShaderResource::ResetData()
{
  m_load_data = std::make_shared<Data>();
  m_processing_load_data = false;
}

Resource::TaskComplete ShaderResource::CollectPrimaryData()
{
  const auto shader_data = m_shader_asset->GetData();
  if (!shader_data) [[unlikely]]
  {
    return Resource::TaskComplete::No;
  }
  m_load_data->m_shader_data = shader_data;

  return Resource::TaskComplete::Yes;
}

Resource::TaskComplete ShaderResource::ProcessData()
{
  if (!m_processing_load_data)
  {
    std::string_view preprocessor_settings = m_preprocessor_settings;
    auto work = [resource_data = m_load_data, uid = &m_uid, bits = m_shader_host_config.bits,
                 preprocessor_settings = preprocessor_settings] {
      ShaderHostConfig shader_host_config;
      shader_host_config.bits = bits;
      auto actual_uid = ApplyDriverBugs(*uid);

      ClearUnusedPixelShaderUidBits(g_backend_info.api_type, shader_host_config,
                                    &actual_uid.ps_uid);
      resource_data->m_needs_geometry_shader = shader_host_config.backend_geometry_shaders &&
                                               !actual_uid.gs_uid.GetUidData()->IsPassthrough();

      if (resource_data->m_needs_geometry_shader)
      {
        resource_data->m_geometry_shader =
            CompileGeometryShader(actual_uid.gs_uid, g_backend_info.api_type, shader_host_config);
      }
      resource_data->m_pixel_shader =
          CompilePixelShader(actual_uid.ps_uid, preprocessor_settings, g_backend_info.api_type,
                             shader_host_config, *resource_data->m_shader_data);
      resource_data->m_vertex_shader =
          CompileVertexShader(actual_uid.vs_uid, preprocessor_settings, g_backend_info.api_type,
                              shader_host_config, *resource_data->m_shader_data);
      resource_data->m_processing_finished = true;
    };
    m_worker_queue->Push(std::move(work));
    m_processing_load_data = true;
  }

  if (!m_load_data->m_processing_finished)
    return Resource::TaskComplete::No;

  if (!m_load_data->IsCompiled())
    return Resource::TaskComplete::Error;

  std::swap(m_current_data, m_load_data);
  return Resource::TaskComplete::Yes;
}
}  // namespace VideoCommon
