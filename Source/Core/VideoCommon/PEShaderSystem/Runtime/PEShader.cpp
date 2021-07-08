// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Runtime/PEShader.h"

#include <algorithm>

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderOption.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderOutput.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon::PE
{
bool VertexPixelShader::CreatePasses(const ShaderConfig& config)
{
  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders && !m_geometry_shader)
  {
    m_geometry_shader = CompileGeometryShader(config);
    if (!m_geometry_shader)
      return false;
  }

  std::shared_ptr<AbstractShader> vertex_shader = CompileVertexShader(config);
  if (!vertex_shader)
  {
    config.m_runtime_info->SetError(true);
    return false;
  }

  m_passes.clear();
  for (const auto& config_pass : config.m_passes)
  {
    bool should_skip_pass = false;
    if (config_pass.m_dependent_option != "")
    {
      for (const auto& option : config.m_options)
      {
        if (IsUnsatisfiedDependency(option, config_pass.m_dependent_option))
        {
          should_skip_pass = true;
          break;
        }
      }
    }

    if (should_skip_pass)
    {
      continue;
    }
    auto pass = std::make_unique<ShaderPass>();

    pass->m_inputs.reserve(config_pass.m_inputs.size());
    for (const auto& config_input : config_pass.m_inputs)
    {
      auto input = ShaderInput::Create(config_input, m_outputs);
      if (!input)
        return false;

      pass->m_inputs.push_back(std::move(input));
    }

    for (const auto& config_output : config_pass.m_outputs)
    {
      const auto [it, added] = m_outputs.try_emplace(config_output.m_name, ShaderOutput{});
      it->second.m_name = config_output.m_name;

      pass->m_additional_outputs.push_back(&it->second);
    }

    pass->m_output_scale = config_pass.m_output_scale;
    pass->m_vertex_shader = vertex_shader;
    pass->m_pixel_shader = CompilePixelShader(config, *pass, config_pass);
    if (!pass->m_pixel_shader)
    {
      config.m_runtime_info->SetError(true);
      return false;
    }

    m_passes.push_back(std::move(pass));
  }

  if (m_passes.empty())
    return false;

  return true;
}

bool VertexPixelShader::CreateAllPassOutputTextures(u32 width, u32 height, u32 layers,
                                                    AbstractTextureFormat format)
{
  const auto create_render_target = [&](float output_scale, const std::string& name) {
    u32 output_width;
    u32 output_height;
    if (output_scale < 0.0f)
    {
      // Negative scale means scale to native first
      const float native_scale = -output_scale;
      const int native_width = width * EFB_WIDTH / g_renderer->GetTargetWidth();
      const int native_height = height * EFB_HEIGHT / g_renderer->GetTargetHeight();
      output_width = std::max(
          static_cast<u32>(std::round((static_cast<float>(native_width) * native_scale))), 1u);
      output_height = std::max(
          static_cast<u32>(std::round((static_cast<float>(native_height) * native_scale))), 1u);
    }
    else
    {
      output_width = std::max(1u, static_cast<u32>(width * output_scale));
      output_height = std::max(1u, static_cast<u32>(height * output_scale));
    }
    return g_renderer->CreateTexture(TextureConfig(output_width, output_height, 1, layers, 1,
                                                   format, AbstractTextureFlag_RenderTarget),
                                     name);
  };

  for (auto& [name, output] : m_outputs)
  {
    output.m_texture =
        create_render_target(output.m_output_scale, fmt::format("Output {}", output.m_name));
    if (!output.m_texture)
    {
      const auto& output_name = output.m_name;
      PanicAlertFmt("Failed to create texture for additional output '{}'", output_name);
      return false;
    }
  }

  std::size_t i = 0;
  for (auto& pass : m_passes)
  {
    auto* native_pass = static_cast<ShaderPass*>(pass.get());
    native_pass->m_output_framebuffer.reset();
    pass->m_output_texture.reset();
    pass->m_output_texture = create_render_target(pass->m_output_scale, fmt::format("Pass {}", i));
    if (!pass->m_output_texture)
    {
      PanicAlertFmt("Failed to create texture for PE shader pass");
      return false;
    }
    std::vector<AbstractTexture*> additional_color_attachments;
    for (auto output : pass->m_additional_outputs)
    {
      additional_color_attachments.push_back(output->m_texture.get());
    }
    native_pass->m_output_framebuffer = g_renderer->CreateFramebuffer(
        pass->m_output_texture.get(), nullptr, std::move(additional_color_attachments));
    if (!native_pass->m_output_framebuffer)
    {
      PanicAlertFmt("Failed to create framebuffer for PE shader pass");
      return false;
    }
    i++;
  }

  return true;
}

bool VertexPixelShader::RebuildPipeline(AbstractTextureFormat format, u32 layers)
{
  AbstractPipelineConfig config = {};
  config.geometry_shader =
      g_renderer->UseGeometryShaderForUI() ? g_shader_cache->GetTexcoordGeometryShader() : nullptr;
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(format);
  config.usage = AbstractPipelineUsage::Utility;

  m_last_pipeline_format = format;
  m_last_pipeline_layers = layers;

  for (auto& pass : m_passes)
  {
    auto* native_pass = static_cast<ShaderPass*>(pass.get());
    config.vertex_shader = native_pass->m_vertex_shader.get();
    config.pixel_shader = native_pass->m_pixel_shader.get();
    config.framebuffer_state.additional_color_attachment_count =
        native_pass->m_additional_outputs.size();
    native_pass->m_pipeline = g_renderer->CreatePipeline(config);
    if (!native_pass->m_pipeline)
    {
      PanicAlertFmt("Failed to compile pipeline for PE shader pass");
      return false;
    }
  }

  return true;
}

void VertexPixelShader::Apply(bool skip_final_copy, const ShaderApplyOptions& options,
                              const AbstractTexture* prev_shader_texture,
                              float prev_pass_output_scale)
{
  const auto parse_inputs = [&](const ShaderPass& pass, const AbstractTexture* prev_pass_texture) {
    for (auto&& input : pass.m_inputs)
    {
      switch (input->GetType())
      {
      case InputType::ColorBuffer:
        g_renderer->SetTexture(input->GetTextureUnit(), options.m_source_color_tex);
        break;

      case InputType::DepthBuffer:
        g_renderer->SetTexture(input->GetTextureUnit(), options.m_source_depth_tex);
        break;

      case InputType::ExternalImage:
      case InputType::NamedOutput:
        g_renderer->SetTexture(input->GetTextureUnit(), input->GetTexture());
        break;
      case InputType::PreviousPassOutput:
        g_renderer->SetTexture(input->GetTextureUnit(), prev_pass_texture);
        break;
      case InputType::PreviousShaderOutput:
        g_renderer->SetTexture(input->GetTextureUnit(), prev_shader_texture);
        break;
      }

      g_renderer->SetSamplerState(input->GetTextureUnit(), input->GetSamplerState());
    }
  };

  const AbstractTexture* last_pass_texture = prev_shader_texture;
  float last_pass_output_scale = prev_pass_output_scale;
  for (std::size_t i = 0; i < m_passes.size() - 1; i++)
  {
    auto& pass = m_passes[i];
    auto* native_pass = static_cast<ShaderPass*>(pass.get());
    g_renderer->SetAndDiscardFramebuffer(native_pass->m_output_framebuffer.get());
    g_renderer->SetPipeline(native_pass->m_pipeline.get());
    g_renderer->SetViewportAndScissor(g_renderer->ConvertFramebufferRectangle(
        native_pass->m_output_framebuffer->GetRect(), native_pass->m_output_framebuffer.get()));

    parse_inputs(*native_pass, last_pass_texture);

    m_builtin_uniforms.Update(options, last_pass_texture, last_pass_output_scale);
    UploadUniformBuffer();

    g_renderer->Draw(0, 3);
    for (auto* attachment : native_pass->m_additional_outputs)
    {
      attachment->m_texture->FinishedRendering();
    }
    native_pass->m_output_framebuffer->GetColorAttachment()->FinishedRendering();

    last_pass_texture = pass->m_output_texture.get();
    last_pass_output_scale = pass->m_output_scale;
  }

  const auto& pass = m_passes[m_passes.size() - 1];
  const auto* native_pass = static_cast<ShaderPass*>(pass.get());
  if (skip_final_copy)
  {
    g_renderer->SetFramebuffer(options.m_dest_fb);
    g_renderer->SetPipeline(native_pass->m_pipeline.get());
    g_renderer->SetViewportAndScissor(
        g_renderer->ConvertFramebufferRectangle(options.m_dest_rect, options.m_dest_fb));

    parse_inputs(*native_pass, last_pass_texture);

    m_builtin_uniforms.Update(options, last_pass_texture, last_pass_output_scale);
    UploadUniformBuffer();

    g_renderer->Draw(0, 3);
  }
  else
  {
    g_renderer->SetAndDiscardFramebuffer(native_pass->m_output_framebuffer.get());
    g_renderer->SetPipeline(native_pass->m_pipeline.get());
    g_renderer->SetViewportAndScissor(g_renderer->ConvertFramebufferRectangle(
        native_pass->m_output_framebuffer->GetRect(), native_pass->m_output_framebuffer.get()));

    parse_inputs(*native_pass, last_pass_texture);

    m_builtin_uniforms.Update(options, last_pass_texture, last_pass_output_scale);
    UploadUniformBuffer();

    g_renderer->Draw(0, 3);
    for (auto* attachment : native_pass->m_additional_outputs)
    {
      attachment->m_texture->FinishedRendering();
    }
    native_pass->m_output_framebuffer->GetColorAttachment()->FinishedRendering();
  }
}

bool VertexPixelShader::RecompilePasses(const ShaderConfig& config)
{
  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders && !m_geometry_shader)
  {
    m_geometry_shader = CompileGeometryShader(config);
    if (!m_geometry_shader)
      return false;
  }

  std::shared_ptr<AbstractShader> vertex_shader = CompileVertexShader(config);
  if (!vertex_shader)
  {
    config.m_runtime_info->SetError(true);
    return false;
  }

  for (std::size_t i = 0; i < m_passes.size(); i++)
  {
    auto& pass = m_passes[i];
    auto* native_pass = static_cast<ShaderPass*>(pass.get());
    auto& config_pass = config.m_passes[i];

    native_pass->m_vertex_shader = vertex_shader;
    native_pass->m_pixel_shader = CompilePixelShader(config, *native_pass, config_pass);
    if (!native_pass->m_pixel_shader)
    {
      config.m_runtime_info->SetError(true);
      return false;
    }
  }

  if (!RebuildPipeline(m_last_pipeline_format, m_last_pipeline_layers))
    return false;

  return true;
}

std::unique_ptr<AbstractShader>
VertexPixelShader::CompileGeometryShader(const ShaderConfig& config) const
{
  std::string source = FramebufferShaderGen::GeneratePassthroughGeometryShader(2, 0);
  return g_renderer->CreateShaderFromSource(ShaderStage::Geometry, std::move(source),
                                            fmt::format("{} gs", config.m_name));
}

std::shared_ptr<AbstractShader>
VertexPixelShader::CompileVertexShader(const ShaderConfig& config) const
{
  ShaderCode shader_source;
  PrepareUniformHeader(shader_source);
  VertexShaderMain(shader_source);

  std::unique_ptr<AbstractShader> vs = g_renderer->CreateShaderFromSource(
      ShaderStage::Vertex, shader_source.GetBuffer(), fmt::format("{} vs", config.m_name));
  if (!vs)
  {
    return nullptr;
  }

  // convert from unique_ptr to shared_ptr, as many passes share one VS
  return std::shared_ptr<AbstractShader>(vs.release());
}

std::unique_ptr<AbstractShader>
VertexPixelShader::CompilePixelShader(const ShaderConfig& config, const ShaderPass& pass,
                                      const ShaderConfigPass& config_pass) const
{
  ShaderCode shader_source;
  PrepareUniformHeader(shader_source);
  PixelShaderHeader(shader_source, pass);

  std::string shader_body;
  File::ReadFileToString(config.m_shader_path, shader_body);
  shader_body = ReplaceAll(shader_body, "\r\n", "\n");
  shader_body = ReplaceAll(shader_body, "{", "{{");
  shader_body = ReplaceAll(shader_body, "}", "}}");

  shader_source.Write(fmt::runtime(shader_body));
  shader_source.Write("\n");
  PixelShaderFooter(shader_source, config_pass);
  return g_renderer->CreateShaderFromSource(ShaderStage::Pixel, shader_source.GetBuffer(),
                                            fmt::format("{} ps", config.m_name));
}

void VertexPixelShader::PixelShaderHeader(ShaderCode& shader_source, const ShaderPass& pass) const
{
  shader_source.Write(R"(
#define GLSL 1

// Type aliases.
#define float2x2 mat2
#define float3x3 mat3
#define float4x4 mat4
#define float4x3 mat4x3

// Utility functions.
float saturate(float x) {{ return clamp(x, 0.0f, 1.0f); }}
float2 saturate(float2 x) {{ return clamp(x, float2(0.0f, 0.0f), float2(1.0f, 1.0f)); }}
float3 saturate(float3 x) {{ return clamp(x, float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f)); }}
float4 saturate(float4 x) {{ return clamp(x, float4(0.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 1.0f, 1.0f, 1.0f)); }}

// Flipped multiplication order because GLSL matrices use column vectors.
float2 mul(float2x2 m, float2 v) {{ return (v * m); }}
float3 mul(float3x3 m, float3 v) {{ return (v * m); }}
float4 mul(float4x3 m, float3 v) {{ return (v * m); }}
float4 mul(float4x4 m, float4 v) {{ return (v * m); }}
float2 mul(float2 v, float2x2 m) {{ return (m * v); }}
float3 mul(float3 v, float3x3 m) {{ return (m * v); }}
float3 mul(float4 v, float4x3 m) {{ return (m * v); }}
float4 mul(float4 v, float4x4 m) {{ return (m * v); }}

float4x3 mul(float4x3 m, float3x3 m2) {{ return (m2 * m); }}

SAMPLER_BINDING(0) uniform sampler2DArray samp[{0}];
FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;
)",
                      pass.m_inputs.size());

  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
  {
    shader_source.Write("VARYING_LOCATION(0) in VertexData {{\n");
    shader_source.Write("  float3 v_tex0;\n");
    shader_source.Write("  float3 v_tex1;\n");
    shader_source.Write("}};\n");
  }
  else
  {
    shader_source.Write("VARYING_LOCATION(0) in float3 v_tex0;\n");
    shader_source.Write("VARYING_LOCATION(1) in float3 v_tex1;\n");
  }

  if (pass.m_additional_outputs.empty())
  {
    shader_source.Write("void SetAdditionalOutput(int value, float4 color) {{}}\n");
  }
  else
  {
    for (std::size_t i = 0; i < pass.m_additional_outputs.size(); i++)
    {
      shader_source.Write("FRAGMENT_OUTPUT_LOCATION({0}) out float4 ocolo{0};\n", i + 1);
    }

    shader_source.Write("void SetAdditionalOutput(int value, float4 color) {{\n");
    for (std::size_t i = 0; i < pass.m_additional_outputs.size(); i++)
    {
      if (i == 0)
      {
        shader_source.Write("\tif (value == {0})\n", i);
      }
      else
      {
        shader_source.Write("\telse if (value == {0})\n", i);
      }
      shader_source.Write("\t{{\n");
      shader_source.Write("\t\tocolo{0} = color;\n", i + 1);
      shader_source.Write("\t}}\n");
    }
    shader_source.Write("}}\n\n");
  }

  shader_source.Write(R"(

// Wrappers for sampling functions.
float4 SampleInput(int value) {{ return texture(samp[value], float3(v_tex0.xy, float(u_layer))); }}
float4 SampleInputLocation(int value, float2 location) {{ return texture(samp[value], float3(location, float(u_layer))); }}
float4 SampleInputLayer(int value, int layer) {{ return texture(samp[value], float3(v_tex0.xy, float(layer))); }}
float4 SampleInputLocationLayer(int value, float2 location, int layer) {{ return texture(samp[value], float3(location, float(layer))); }}

float4 SampleInputLod(int value, float lod) {{ return textureLod(samp[value], float3(v_tex0.xy, float(u_layer)), lod); }}
float4 SampleInputLodLocation(int value, float lod, float2 location) {{ return textureLod(samp[value], float3(location, float(u_layer)), lod); }}
float4 SampleInputLodLayer(int value, float lod, int layer) {{ return textureLod(samp[value], float3(v_tex0.xy, float(layer)), lod); }}
float4 SampleInputLodLocationLayer(int value, float lod, float2 location, int layer) {{ return textureLod(samp[value], float3(location, float(layer)), lod); }}

// In Vulkan, offset can only be a compile time constant
// so instead of using a function, we use a macro
#define SampleInputOffset(value,  offset) (textureOffset(samp[value], float3(v_tex0.xy, float(u_layer)), offset))
#define SampleInputLocationOffset(value, location, offset) (textureOffset(samp[value], float3(location.xy, float(u_layer)), offset))
#define SampleInputLayerOffset(value, layer, offset) (textureOffset(samp[value], float3(v_tex0.xy, float(layer)), offset))
#define SampleInputLocationLayerOffset(value, location, layer, offset) (textureOffset(samp[value], float3(location.xy, float(layer)), offset))

#define SampleInputLodOffset(value, lod, offset) (textureLodOffset(samp[value], float3(v_tex0.xy, float(u_layer)), lod, offset))
#define SampleInputLodLocationOffset(value, lod, location, offset) (textureLodOffset(samp[value], float3(location.xy, float(u_layer)), lod, offset))
#define SampleInputLodLayerOffset(value, lod, layer, offset) (textureLodOffset(samp[value], float3(v_tex0.xy, float(layer)), lod, offset))
#define SampleInputLodLocationLayerOffset(value, lod, location, layer, offset) (textureLodOffset(samp[value], float3(location.xy, float(layer)), lod, offset))

ivec3 SampleInputSize(int value, int lod) {{ return textureSize(samp[value], lod); }}
)");

  if (g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
    shader_source.Write("#define DEPTH_VALUE(val) (val)\n");
  else
    shader_source.Write("#define DEPTH_VALUE(val) (1.0 - (val))\n");

  std::vector<std::string> output_keys;
  output_keys.reserve(m_outputs.size());
  for (const auto& [k, v] : m_outputs)
  {
    output_keys.push_back(k);
  }
  pass.WriteShaderIndices(shader_source, output_keys);

  for (const auto& option : m_options)
  {
    option->WriteShaderConstants(shader_source);
  }

  shader_source.Write(R"(

// Convert z/w -> linear depth
// https://gist.github.com/kovrov/a26227aeadde77b78092b8a962bd1a91
// http://dougrogers.blogspot.com/2013/02/how-to-derive-near-and-far-clip-plane.html
float ToLinearDepth(float depth)
{{
  // TODO: The depth values provided by the projection matrix
  // produce invalid results, need to determine what the correct
  // values are
  //float NearZ = z_depth_near;
  //float FarZ = z_depth_far;
  //return z_depth_linear_mul / (z_depth_linear_add - DEPTH_VALUE(depth));

  // For now just hardcode our near/far planes
  float NearZ = 0.001f;
  float FarZ = 1.0f;
  const float A = (1.0f - (FarZ / NearZ)) / 2.0f;
  const float B = (1.0f + (FarZ / NearZ)) / 2.0f;
  return 1.0f / (A * depth + B);
}}

// For backwards compatibility.
float4 Sample() {{ return SampleInput(PREV_PASS_OUTPUT_INPUT_INDEX); }}
float4 SampleLocation(float2 location) {{ return SampleInputLocation(PREV_PASS_OUTPUT_INPUT_INDEX, location); }}
float4 SampleLayer(int layer) {{ return SampleInputLayer(PREV_PASS_OUTPUT_INPUT_INDEX, layer); }}
float4 SamplePrev() {{ return SampleInput(PREV_PASS_OUTPUT_INPUT_INDEX); }}
float4 SamplePrevLocation(float2 location) {{ return SampleInputLocation(PREV_PASS_OUTPUT_INPUT_INDEX, location); }}
float SampleRawDepth() {{ return DEPTH_VALUE(SampleInput(DEPTH_BUFFER_INPUT_INDEX).x); }}
float SampleRawDepthLocation(float2 location) {{ return DEPTH_VALUE(SampleInputLocation(DEPTH_BUFFER_INPUT_INDEX, location).x); }}
float SampleDepth() {{ return ToLinearDepth(SampleRawDepth()); }}
float SampleDepthLocation(float2 location) {{ return ToLinearDepth(SampleRawDepthLocation(location)); }}
#define SampleOffset(offset) (SampleInputOffset(COLOR_BUFFER_INPUT_INDEX, offset))
#define SampleLayerOffset(offset, layer) (SampleInputLayerOffset(COLOR_BUFFER_INPUT_INDEX, layer, offset))
#define SamplePrevOffset(offset) (SampleInputOffset(PREV_PASS_OUTPUT_INPUT_INDEX, offset))
#define SampleRawDepthOffset(offset) (DEPTH_VALUE(SampleInputOffset(DEPTH_BUFFER_INPUT_INDEX, offset).x))
#define SampleDepthOffset(offset) (ToLinearDepth(SampleRawDepthOffset(offset)))

float2 GetResolution() {{ return prev_resolution.xy; }}
float2 GetInvResolution() {{ return prev_resolution.zw; }}
float2 GetWindowResolution() {{  return window_resolution.xy; }}
float2 GetInvWindowResolution() {{ return window_resolution.zw; }}
float2 GetCoordinates() {{ return v_tex0.xy; }}

float2 GetPrevResolution() {{ return prev_resolution.xy; }}
float2 GetInvPrevResolution() {{ return prev_resolution.zw; }}
float2 GetPrevRectOrigin() {{ return prev_rect.xy; }}
float2 GetPrevRectSize() {{ return prev_rect.zw; }}
float2 GetPrevCoordinates() {{ return v_tex0.xy; }}

float2 GetSrcResolution() {{ return src_resolution.xy; }}
float2 GetInvSrcResolution() {{ return src_resolution.zw; }}
float2 GetSrcRectOrigin() {{ return src_rect.xy; }}
float2 GetSrcRectSize() {{ return src_rect.zw; }}
float2 GetSrcCoordinates() {{ return v_tex1.xy; }}

float4 GetFragmentCoord() {{ return gl_FragCoord; }}

float GetLayer() {{ return u_layer; }}
float GetTime() {{ return u_time; }}

void SetOutput(float4 color) {{ ocol0 = color; }}

#define GetOption(x) (x)
#define OptionEnabled(x) (x)

)");
}

void VertexPixelShader::PixelShaderFooter(ShaderCode& shader_source,
                                          const ShaderConfigPass& config_pass) const
{
  if (config_pass.m_entry_point.empty())
  {
    // No entry point should create a copy
    shader_source.Write("void main() {{ ocol0 = SampleInput(0);\n }}\n");
  }
  else if (config_pass.m_entry_point != "main")
  {
    shader_source.Write("void main()\n");
    shader_source.Write("{{\n");
    shader_source.Write("\t{}();\n", config_pass.m_entry_point);
    shader_source.Write("}}\n");
  }
}
void VertexPixelShader::VertexShaderMain(ShaderCode& shader_source) const
{
  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
  {
    shader_source.Write("VARYING_LOCATION(0) out VertexData {{\n");
    shader_source.Write("  float3 v_tex0;\n");
    shader_source.Write("  float3 v_tex1;\n");
    shader_source.Write("}};\n");
  }
  else
  {
    shader_source.Write("VARYING_LOCATION(0) out float3 v_tex0;\n");
    shader_source.Write("VARYING_LOCATION(1) out float3 v_tex1;\n");
  }

  shader_source.Write("#define id gl_VertexID\n");
  shader_source.Write("#define opos gl_Position\n");
  shader_source.Write("void main() {{\n");
  shader_source.Write("  v_tex0 = float3(float((id << 1) & 2), float(id & 2), 0.0f);\n");
  shader_source.Write(
      "  opos = float4(v_tex0.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n");
  shader_source.Write("  v_tex1 = float3(src_rect.xy + (src_rect.zw * v_tex0.xy), 0.0f);\n");

  // NDC space is flipped in Vulkan
  if (g_ActiveConfig.backend_info.api_type == APIType::Vulkan)
  {
    shader_source.Write("  opos.y = -opos.y;\n");
  }

  shader_source.Write("}}\n");
}
}  // namespace VideoCommon::PE
