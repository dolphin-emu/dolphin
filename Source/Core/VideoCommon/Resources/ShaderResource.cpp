// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/ShaderResource.h"

#include <string_view>

#include <fmt/format.h>

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PipelineUtils.h"
#include "VideoCommon/PixelShaderGen.h"
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
  return g_gfx->CreateShaderFromSource(ShaderStage::Geometry, source_code.GetBuffer(), nullptr,
                                       fmt::format("Geometry shader: {}", *uid.GetUidData()));
}

std::unique_ptr<AbstractShader> CompilePixelShader(const PixelShaderUid& uid,
                                                   std::string_view preprocessor_settings,
                                                   APIType api_type,
                                                   const ShaderHostConfig& host_config,
                                                   RasterSurfaceShaderData* shader_data)
{
  ShaderCode shader_code;

  // Write any preprocessor values that were passed in
  shader_code.Write("{}", preprocessor_settings);

  // TODO: in the future we could dynamically determine the amount of samplers
  // available, for now just hardcode to start at 8 (the first non game
  // sampler index available)
  const std::size_t custom_sampler_index_offset = 8;
  for (std::size_t i = 0; i < shader_data->samplers.size(); i++)
  {
    const auto& sampler = shader_data->samplers[i];
    std::string_view sampler_type;
    switch (sampler.type)
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
                      sampler_type, sampler.name);

    // Sampler usage is passed in from the material
    // Write a new preprocessor value with the sampler name
    // for easier code in the shader
    shader_code.Write("#if HAS_SAMPLER_{} == 1\n", i);
    shader_code.Write("#define HAS_{} 1\n", sampler.name);
    shader_code.Write("#endif\n");

    shader_code.Write("\n");
  }
  shader_code.Write("\n");

  // Now write the custom shader
  shader_code.Write("{}", ReplaceAll(shader_data->pixel_source, "\r\n", "\n"));

  // Write out the uniform data
  ShaderCode uniform_code;
  for (const auto& property : shader_data->uniform_properties)
  {
    VideoCommon::ShaderProperty::WriteAsShaderCode(uniform_code, property);
  }
  if (!shader_data->uniform_properties.empty())
    uniform_code.Write("\n\n");

  // Compile the shader
  CustomPixelContents contents{.shader = shader_code.GetBuffer(),
                               .uniforms = uniform_code.GetBuffer()};
  const ShaderCode source_code =
      GeneratePixelShaderCode(api_type, host_config, uid.GetUidData(), contents);
  ShaderIncluder* shader_includer =
      shader_data->shader_includer ? &*shader_data->shader_includer : nullptr;
  return g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer(), shader_includer,
                                       "Custom Pixel Shader");
}

std::unique_ptr<AbstractShader> CompileVertexShader(const VertexShaderUid& uid,
                                                    std::string_view preprocessor_settings,
                                                    APIType api_type,
                                                    const ShaderHostConfig& host_config,
                                                    const RasterSurfaceShaderData& shader_data)
{
  ShaderCode shader_code;

  // Write any preprocessor values that were passed in
  shader_code.Write("{}", preprocessor_settings);

  // TODO: in the future we could dynamically determine the amount of samplers
  // available, for now just hardcode to start at 8 (the first non game
  // sampler index available)
  const std::size_t custom_sampler_index_offset = 8;
  for (std::size_t i = 0; i < shader_data.samplers.size(); i++)
  {
    const auto& sampler = shader_data.samplers[i];
    std::string_view sampler_type = "";
    switch (sampler.type)
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
                      sampler_type, sampler.name);

    // Sampler usage is passed in from the material
    // Write a new preprocessor value with the sampler name
    // for easier code in the shader
    shader_code.Write("#if HAS_SAMPLER_{} == 1\n", i);
    shader_code.Write("#define HAS_{} 1\n", sampler.name);
    shader_code.Write("#endif\n");

    shader_code.Write("\n");
  }
  shader_code.Write("\n");

  // Now write the custom shader
  shader_code.Write("{}", ReplaceAll(shader_data.vertex_source, "\r\n", "\n"));

  // Write out the uniform data
  ShaderCode uniform_code;
  for (const auto& property : shader_data.uniform_properties)
  {
    VideoCommon::ShaderProperty::WriteAsShaderCode(uniform_code, property);
  }
  if (!shader_data.uniform_properties.empty())
    uniform_code.Write("\n\n");

  // Compile the shader
  CustomVertexContents contents{.shader = shader_code.GetBuffer(),
                                .uniforms = uniform_code.GetBuffer()};
  const ShaderCode source_code =
      GenerateVertexShaderCode(api_type, host_config, uid.GetUidData(), contents);
  return g_gfx->CreateShaderFromSource(ShaderStage::Vertex, source_code.GetBuffer(), nullptr,
                                       "Custom Vertex Shader");
}
}  // namespace
ShaderResource::ShaderResource(Resource::ResourceContext resource_context,
                               const GXPipelineUid& pipeline_uid,
                               const std::string& preprocessor_setting,
                               const ShaderHostConfig& shader_host_config)
    : Resource(std::move(resource_context)), m_shader_host_config{.bits = shader_host_config.bits},
      m_uid(pipeline_uid), m_preprocessor_settings(preprocessor_setting)
{
  m_shader_asset = m_resource_context.asset_cache->CreateAsset<RasterSurfaceShaderAsset>(
      m_resource_context.primary_asset_id, m_resource_context.asset_library, this);
}

void ShaderResource::SetHostConfig(const ShaderHostConfig& host_config)
{
  m_shader_host_config.bits = host_config.bits;
}

void ShaderResource::MarkAsPending()
{
  m_resource_context.asset_cache->MarkAssetPending(m_shader_asset);
}

void ShaderResource::MarkAsActive()
{
  m_resource_context.asset_cache->MarkAssetActive(m_shader_asset);
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

AbstractTextureType ShaderResource::Data::GetTextureType(std::size_t index)
{
  // If the data doesn't exist, just pick one...
  if (!m_shader_data || index >= m_shader_data->samplers.size()) [[unlikely]]
    return AbstractTextureType::Texture_2D;

  return m_shader_data->samplers[index].type;
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
  class WorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    WorkItem(std::shared_ptr<ShaderResource::Data> resource_data, VideoCommon::GXPipelineUid* uid,
             u32 shader_bits, std::string_view preprocessor_settings)
        : m_resource_data(std::move(resource_data)), m_uid(uid), m_shader_bits(shader_bits),
          m_preprocessor_settings(preprocessor_settings)
    {
    }

    bool Compile() override
    {
      const ShaderHostConfig shader_host_config{.bits = m_shader_bits};
      auto actual_uid = ApplyDriverBugs(*m_uid);

      ClearUnusedPixelShaderUidBits(g_backend_info.api_type, shader_host_config,
                                    &actual_uid.ps_uid);
      m_resource_data->m_needs_geometry_shader = shader_host_config.backend_geometry_shaders &&
                                                 !actual_uid.gs_uid.GetUidData()->IsPassthrough();

      if (m_resource_data->m_needs_geometry_shader)
      {
        m_resource_data->m_geometry_shader =
            CompileGeometryShader(actual_uid.gs_uid, g_backend_info.api_type, shader_host_config);
      }
      m_resource_data->m_pixel_shader =
          CompilePixelShader(actual_uid.ps_uid, m_preprocessor_settings, g_backend_info.api_type,
                             shader_host_config, m_resource_data->m_shader_data.get());
      m_resource_data->m_vertex_shader =
          CompileVertexShader(actual_uid.vs_uid, m_preprocessor_settings, g_backend_info.api_type,
                              shader_host_config, *m_resource_data->m_shader_data);
      m_resource_data->m_processing_finished = true;
      return true;
    }
    void Retrieve() override {}

  private:
    std::shared_ptr<ShaderResource::Data> m_resource_data;
    VideoCommon::GXPipelineUid* m_uid;
    u32 m_shader_bits;
    std::string_view m_preprocessor_settings;
  };

  if (!m_processing_load_data)
  {
    std::string_view preprocessor_settings = m_preprocessor_settings;
    auto wi = m_resource_context.shader_compiler->CreateWorkItem<WorkItem>(
        m_load_data, &m_uid, m_shader_host_config.bits, preprocessor_settings);

    // We don't need priority, that is already handled by the resource system
    m_resource_context.shader_compiler->QueueWorkItem(std::move(wi), 0);
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
