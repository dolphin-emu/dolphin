// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/ShaderResource.h"

#include <string_view>

#include <fmt/format.h>

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PipelineUtils.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
namespace
{
// TODO: the uniform buffer is combined due to the utility path only having a single
// set of constants, this isn't ideal (both for the end user, where it's more readable
// to see 'custom_uniforms' before variables and from Dolphin because the
// standard uniforms have to be packed with the custom ones
void GeneratePostProcessUniformOutput(ShaderCode& shader_source, std::string_view block_name,
                                      std::string_view uniforms)
{
  shader_source.Write("UBO_BINDING(std140, 1) uniform {} {{\n", block_name);
  shader_source.Write("\tvec4 source_resolution;\n");
  shader_source.Write("\tvec4 target_resolution;\n");
  shader_source.Write("\tvec4 window_resolution;\n");
  shader_source.Write("\tvec4 source_region;\n");
  shader_source.Write("\tint source_layer;\n");
  shader_source.Write("\tint source_layer_pad1;\n");
  shader_source.Write("\tint source_layer_pad2;\n");
  shader_source.Write("\tint source_layer_pad3;\n");
  shader_source.Write("\tuint time;\n");
  shader_source.Write("\tuint time_pad1;\n");
  shader_source.Write("\tuint time_pad2;\n");
  shader_source.Write("\tuint time_pad3;\n");
  shader_source.Write("\tint graphics_api;\n");
  shader_source.Write("\tint graphics_api_pad1;\n");
  shader_source.Write("\tint graphics_api_pad2;\n");
  shader_source.Write("\tint graphics_api_pad3;\n");
  shader_source.Write("\tuint efb_scale;\n");
  shader_source.Write("\tuint efb_scale_pad1;\n");
  shader_source.Write("\tuint efb_scale_pad2;\n");
  shader_source.Write("\tuint efb_scale_pad3;\n");
  if (!uniforms.empty())
  {
    shader_source.Write("{}", uniforms);
  }
  shader_source.Write("}};\n");
}

// TODO: move this to a more standard post processing file
// once post processing has been cleaned up
void GeneratePostProcessingVertexShader(ShaderCode& shader_source,
                                        const CustomVertexContents& custom_contents)
{
  // Note: if blocks are the same, they need to match for the OpenGL backend
  GeneratePostProcessUniformOutput(shader_source, "PSBlock", custom_contents.uniforms);

  // Define some defines that are shared with custom draw shaders,
  // so that a common shader code could possibly be used in both
  shader_source.Write("#define HAS_COLOR_0 0\n");
  shader_source.Write("#define HAS_COLOR_1 0\n");
  shader_source.Write("#define HAS_NORMAL 0\n");
  shader_source.Write("#define HAS_BINORMAL 0\n");
  shader_source.Write("#define HAS_TANGENT 0\n");
  shader_source.Write("#define HAS_TEXTURE_COORD_0 1\n");
  for (u32 i = 1; i < 8; i++)
  {
    shader_source.Write("#define HAS_TEXTURE_COORD_{} 0\n", i);
  }

  // Write the common structs, might want to consider
  // moving these to another location?
  shader_source.Write("struct DolphinVertexInput\n");
  shader_source.Write("{{\n");
  shader_source.Write("\tvec4 position;\n");
  shader_source.Write("\tvec3 texture_coord_0;\n");
  shader_source.Write("}};\n\n");

  shader_source.Write("struct DolphinVertexOutput\n");
  shader_source.Write("{{\n");
  shader_source.Write("\tvec4 position;\n");
  shader_source.Write("\tvec3 texture_coord_0;\n");
  shader_source.Write("}};\n\n");

  constexpr std::string_view emulated_vertex_definition =
      "void dolphin_process_emulated_vertex(in DolphinVertexInput vertex_input, out "
      "DolphinVertexOutput vertex_output)";
  shader_source.Write("{}\n", emulated_vertex_definition);
  shader_source.Write("{{\n");
  shader_source.Write("\tvertex_output.position = vertex_input.position;\n");
  shader_source.Write("\tvertex_output.texture_coord_0 = vertex_input.texture_coord_0;\n");
  shader_source.Write("}}\n");

  if (custom_contents.shader.empty())
  {
    shader_source.Write(
        "void process_vertex(in DolphinVertexInput vertex_input, out DolphinVertexOutput "
        "vertex_output)\n");
    shader_source.Write("{{\n");

    shader_source.Write("\tdolphin_process_emulated_vertex(vertex_input, vertex_output);\n");

    shader_source.Write("}}\n");
  }
  else
  {
    shader_source.Write("{}\n", custom_contents.shader);
  }

  if (g_backend_info.bSupportsGeometryShaders)
  {
    shader_source.Write("VARYING_LOCATION(0) out VertexData {{\n");
    shader_source.Write("\tvec3 v_tex0;\n");
    shader_source.Write("}};\n");
  }
  else
  {
    shader_source.Write("VARYING_LOCATION(0) out vec3 v_tex0;\n");
  }

  shader_source.Write("void main()\n");
  shader_source.Write("{{\n");
  shader_source.Write("\tDolphinVertexInput vertex_input;\n");
  shader_source.Write("\tvec3 vert = vec3(float((gl_VertexID << 1) & 2), "
                      "float(gl_VertexID & 2), 0.0f);\n");
  shader_source.Write("\tvertex_input.position = vec4(vert.xy * "
                      "float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n");
  shader_source.Write("\tvertex_input.texture_coord_0 = vec3(source_region.xy + "
                      "(source_region.zw * vert.xy), 0.0f);\n");
  shader_source.Write("\tDolphinVertexOutput vertex_output;\n");
  shader_source.Write("\tprocess_vertex(vertex_input, vertex_output);\n");
  shader_source.Write("\tgl_Position = vertex_output.position;\n");
  shader_source.Write("\tv_tex0 = vertex_output.texture_coord_0;\n");

  // NDC space is flipped in Vulkan
  if (g_backend_info.api_type == APIType::Vulkan)
  {
    shader_source.Write("\tgl_Position.y = -gl_Position.y;\n");
  }

  shader_source.Write("}}\n");
}

void GeneratePostProcessingPixelShader(ShaderCode& shader_source,
                                       const CustomPixelContents& custom_contents)
{
  GeneratePostProcessUniformOutput(shader_source, "PSBlock", custom_contents.uniforms);

  shader_source.Write("SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n");

  if (g_backend_info.bSupportsGeometryShaders)
  {
    shader_source.Write("VARYING_LOCATION(0) in VertexData {{\n");
    shader_source.Write("\tvec3 v_tex0;\n");
    shader_source.Write("}};\n");
  }
  else
  {
    shader_source.Write("VARYING_LOCATION(0) in float3 v_tex0;\n");
  }

  shader_source.Write("FRAGMENT_OUTPUT_LOCATION(0) out vec4 ocol0;\n");

  shader_source.Write("struct DolphinFragmentInput\n");
  shader_source.Write("{{\n");
  for (u32 i = 0; i < 1; i++)
  {
    shader_source.Write("\tvec3 tex{};\n", i);
  }
  shader_source.Write("\n");

  shader_source.Write("}};\n\n");

  shader_source.Write("struct DolphinFragmentOutput\n");
  shader_source.Write("{{\n");
  shader_source.Write("\tvec4 main;\n");
  shader_source.Write("}};\n\n");

  constexpr std::string_view emulated_fragment_definition =
      "void dolphin_process_emulated_fragment(in DolphinFragmentInput frag_input, out "
      "DolphinFragmentOutput frag_output)";
  shader_source.Write("{}\n", emulated_fragment_definition);
  shader_source.Write("{{\n");
  shader_source.Write("\tfrag_output.main = texture(samp0, frag_input.tex0);\n");
  shader_source.Write("}}\n");

  if (custom_contents.shader.empty())
  {
    shader_source.Write(
        "void process_fragment(in DolphinFragmentInput frag_input, out DolphinFragmentOutput "
        "frag_output)\n");
    shader_source.Write("{{\n");

    shader_source.Write("\tdolphin_process_emulated_fragment(frag_input, frag_output);\n");

    shader_source.Write("}}\n");
  }
  else
  {
    shader_source.Write("{}\n", custom_contents.shader);
  }

  shader_source.Write("void main()\n");
  shader_source.Write("{{\n");
  shader_source.Write("\tDolphinFragmentInput frag_input;\n");
  shader_source.Write("\tfrag_input.tex0 = v_tex0;\n");
  shader_source.Write("\tDolphinFragmentOutput frag_output;\n");
  shader_source.Write("\tprocess_fragment(frag_input, frag_output);\n");
  shader_source.Write("\tocol0 = frag_output.main;\n");
  shader_source.Write("}}\n");
}

std::unique_ptr<AbstractShader> CompileGeometryShader(GeometryShaderUid* uid, APIType api_type,
                                                      ShaderHostConfig host_config)
{
  if (!uid)
  {
    return g_gfx->CreateShaderFromSource(
        ShaderStage::Geometry, FramebufferShaderGen::GeneratePassthroughGeometryShader(1, 0),
        nullptr, "Custom Post Processing Geometry Shader");
  }

  const ShaderCode source_code =
      GenerateGeometryShaderCode(api_type, host_config, uid->GetUidData());
  return g_gfx->CreateShaderFromSource(ShaderStage::Geometry, source_code.GetBuffer(), nullptr,
                                       fmt::format("Geometry shader: {}", *uid->GetUidData()));
}

std::unique_ptr<AbstractShader>
CompilePixelShader(PixelShaderUid* uid, std::string_view preprocessor_settings, APIType api_type,
                   const ShaderHostConfig& host_config, RasterSurfaceShaderData* shader_data)
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

  ShaderCode source_code;
  if (uid)
  {
    source_code = GeneratePixelShaderCode(api_type, host_config, uid->GetUidData(), contents);
  }
  else
  {
    GeneratePostProcessingPixelShader(source_code, contents);
  }
  ShaderIncluder* shader_includer =
      shader_data->shader_includer ? &*shader_data->shader_includer : nullptr;
  return g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer(), shader_includer,
                                       "Custom Pixel Shader");
}

std::unique_ptr<AbstractShader>
CompileVertexShader(VertexShaderUid* uid, std::string_view preprocessor_settings, APIType api_type,
                    const ShaderHostConfig& host_config, const RasterSurfaceShaderData& shader_data)
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

  ShaderCode source_code;
  if (uid)
  {
    source_code = GenerateVertexShaderCode(api_type, host_config, uid->GetUidData(), contents);
  }
  else
  {
    GeneratePostProcessingVertexShader(source_code, contents);
  }
  return g_gfx->CreateShaderFromSource(ShaderStage::Vertex, source_code.GetBuffer(), nullptr,
                                       "Custom Vertex Shader");
}
}  // namespace
ShaderResource::ShaderResource(Resource::ResourceContext resource_context,
                               std::optional<GXPipelineUid> pipeline_uid,
                               const std::string& preprocessor_setting,
                               const ShaderHostConfig& shader_host_config)
    : Resource(std::move(resource_context)), m_shader_host_config{.bits = shader_host_config.bits},
      m_uid(std::move(pipeline_uid)), m_preprocessor_settings(preprocessor_setting)
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
      if (m_uid)
      {
        // Draw based shader
        auto actual_uid = ApplyDriverBugs(*m_uid);

        ClearUnusedPixelShaderUidBits(g_backend_info.api_type, shader_host_config,
                                      &actual_uid.ps_uid);
        m_resource_data->m_needs_geometry_shader = shader_host_config.backend_geometry_shaders &&
                                                   !actual_uid.gs_uid.GetUidData()->IsPassthrough();

        if (m_resource_data->m_needs_geometry_shader)
        {
          m_resource_data->m_geometry_shader = CompileGeometryShader(
              &actual_uid.gs_uid, g_backend_info.api_type, shader_host_config);
        }
        m_resource_data->m_pixel_shader =
            CompilePixelShader(&actual_uid.ps_uid, m_preprocessor_settings, g_backend_info.api_type,
                               shader_host_config, m_resource_data->m_shader_data.get());
        m_resource_data->m_vertex_shader = CompileVertexShader(
            &actual_uid.vs_uid, m_preprocessor_settings, g_backend_info.api_type,
            shader_host_config, *m_resource_data->m_shader_data);
      }
      else
      {
        // Post processing based shader

        m_resource_data->m_needs_geometry_shader =
            shader_host_config.backend_geometry_shaders && shader_host_config.stereo;
        if (m_resource_data->m_needs_geometry_shader)
        {
          m_resource_data->m_geometry_shader =
              CompileGeometryShader(nullptr, g_backend_info.api_type, shader_host_config);
        }
        m_resource_data->m_pixel_shader =
            CompilePixelShader(nullptr, m_preprocessor_settings, g_backend_info.api_type,
                               shader_host_config, m_resource_data->m_shader_data.get());
        m_resource_data->m_vertex_shader =
            CompileVertexShader(nullptr, m_preprocessor_settings, g_backend_info.api_type,
                                shader_host_config, *m_resource_data->m_shader_data);
      }
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
        m_load_data, m_uid ? &*m_uid : nullptr, m_shader_host_config.bits, preprocessor_settings);

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
