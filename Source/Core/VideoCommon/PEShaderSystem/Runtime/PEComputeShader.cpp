// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Runtime/PEComputeShader.h"

#include <algorithm>

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderOption.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon::PE
{
bool ComputeShader::CreatePasses(const ShaderConfig& config)
{
  m_passes.clear();

  // Create all outputs upfront..
  for (const auto& config_pass : config.m_passes)
  {
    for (const auto& config_output : config_pass.m_outputs)
    {
      const auto [it, added] = m_outputs.try_emplace(config_output.m_name, ShaderOutput{});
      it->second.m_name = config_output.m_name;
    }
  }

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
    auto pass = std::make_unique<ComputeShaderPass>();

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
      if (const auto it = m_outputs.find(config_output.m_name); it != m_outputs.end())
        pass->m_additional_outputs.push_back(&it->second);
    }

    pass->m_output_scale = config_pass.m_output_scale;
    pass->m_compute_shader = CompileShader(config, *pass, config_pass);
    if (!pass->m_compute_shader)
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

bool ComputeShader::CreateAllPassOutputTextures(u32 width, u32 height, u32 layers,
                                                AbstractTextureFormat format)
{
  const auto create_render_target = [&](float output_scale, const std::string& name) {
    u32 output_width;
    u32 output_height;
    if (output_scale < 0.0f)
    {
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

  const auto create_compute_target = [&](float output_scale, const std::string& name) {
    u32 output_width;
    u32 output_height;
    if (output_scale < 0.0f)
    {
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
                                                   format, AbstractTextureFlag_ComputeImage),
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

  std::size_t pass_index = 0;
  for (auto& pass : m_passes)
  {
    auto* native_pass = static_cast<ComputeShaderPass*>(pass.get());
    pass->m_output_texture.reset();
    pass->m_output_texture =
        create_render_target(pass->m_output_scale, fmt::format("Pass {}", pass_index));
    if (!native_pass->m_output_texture)
    {
      PanicAlertFmt("Failed to create texture for PE shader pass");
      return false;
    }
    native_pass->m_compute_texture.reset();
    native_pass->m_compute_texture =
        create_compute_target(pass->m_output_scale, fmt::format("Compute Pass {}", pass_index));
    if (!native_pass->m_compute_texture)
    {
      PanicAlertFmt("Failed to create compute texture for PE shader pass");
      return false;
    }
    native_pass->m_additional_compute_textures.clear();
    for (std::size_t i = 0; i < native_pass->m_additional_outputs.size(); i++)
    {
      auto* output = native_pass->m_additional_outputs[i];
      native_pass->m_additional_compute_textures.push_back(
          create_compute_target(pass->m_output_scale, fmt::format("Compute Output {} for Pass {}",
                                                                  output->m_name, pass_index)));
    }
    pass_index++;
  }

  return true;
}

bool ComputeShader::RebuildPipeline(AbstractTextureFormat format, u32 layers)
{
  // Compute shaders don't have any pipelines
  return true;
}

void ComputeShader::Apply(bool skip_final_copy, const ShaderApplyOptions& options,
                          const AbstractTexture* prev_shader_texture, float prev_pass_output_scale)
{
  const auto parse_inputs = [&](const ComputeShaderPass& pass,
                                const AbstractTexture* prev_pass_texture) {
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
  for (const auto& pass : m_passes)
  {
    auto* native_pass = static_cast<ComputeShaderPass*>(pass.get());
    parse_inputs(*native_pass, last_pass_texture);

    m_builtin_uniforms.Update(options, last_pass_texture, last_pass_output_scale);
    UploadUniformBuffer();

    g_renderer->SetComputeImageTexture(0, native_pass->m_compute_texture.get(), false, true);
    u32 compute_image_index = 1;
    for (auto&& compute_texture : native_pass->m_additional_compute_textures)
    {
      g_renderer->SetComputeImageTexture(compute_image_index, compute_texture.get(), false, true);
      compute_image_index++;
    }

    // Should these be exposed?
    const u32 working_group_size = 8;
    g_renderer->DispatchComputeShader(
        native_pass->m_compute_shader.get(), working_group_size, working_group_size, 1,
        native_pass->m_compute_texture->GetWidth() / working_group_size,
        native_pass->m_compute_texture->GetHeight() / working_group_size, 1);

    native_pass->m_compute_texture->FinishedRendering();
    native_pass->m_output_texture->CopyRectangleFromTexture(
        native_pass->m_compute_texture.get(), native_pass->m_compute_texture->GetRect(), 0, 0,
        native_pass->m_output_texture->GetRect(), 0, 0);
    native_pass->m_output_texture->FinishedRendering();
    g_renderer->SetComputeImageTexture(0, nullptr, false, false);
    for (std::size_t i = 0; i < native_pass->m_additional_compute_textures.size(); i++)
    {
      auto* compute_texture = native_pass->m_additional_compute_textures[i].get();
      compute_texture->FinishedRendering();

      auto* output_texture = native_pass->m_additional_outputs[i]->m_texture.get();
      output_texture->CopyRectangleFromTexture(compute_texture, compute_texture->GetRect(), 0, 0,
                                               output_texture->GetRect(), 0, 0);
      output_texture->FinishedRendering();

      g_renderer->SetComputeImageTexture(static_cast<u32>(i) + 1, nullptr, false, false);
    }

    last_pass_texture = native_pass->m_output_texture.get();
    last_pass_output_scale = native_pass->m_output_scale;
  }
}

bool ComputeShader::RecompilePasses(const ShaderConfig& config)
{
  for (std::size_t i = 0; i < m_passes.size(); i++)
  {
    auto& pass = m_passes[i];
    auto* native_pass = static_cast<ComputeShaderPass*>(pass.get());
    auto& config_pass = config.m_passes[i];

    native_pass->m_compute_shader = CompileShader(config, *native_pass, config_pass);
    if (!native_pass->m_compute_shader)
    {
      config.m_runtime_info->SetError(true);
      return false;
    }
  }

  return true;
}

std::unique_ptr<AbstractShader>
ComputeShader::CompileShader(const ShaderConfig& config, const ComputeShaderPass& pass,
                             const ShaderConfigPass& config_pass) const
{
  ShaderCode shader_source;
  PrepareUniformHeader(shader_source);
  ShaderHeader(shader_source, pass);

  std::string shader_body;
  File::ReadFileToString(config.m_shader_path, shader_body);
  shader_body = ReplaceAll(shader_body, "\r\n", "\n");
  shader_body = ReplaceAll(shader_body, "{", "{{");
  shader_body = ReplaceAll(shader_body, "}", "}}");

  shader_source.Write(fmt::runtime(shader_body));
  shader_source.Write("\n");
  ShaderFooter(shader_source, config_pass);
  return g_renderer->CreateShaderFromSource(ShaderStage::Compute, shader_source.GetBuffer(),
                                            fmt::format("{} compute", config.m_name));
}

void ComputeShader::ShaderHeader(ShaderCode& shader_source, const ComputeShaderPass& pass) const
{
  shader_source.Write(R"(
#define GLSL 1
#define main real_main

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
IMAGE_BINDING(rgba8, 0) uniform writeonly image2DArray output_image;
)",
                      pass.m_inputs.size());

  if (pass.m_additional_outputs.empty())
  {
    shader_source.Write(
        "void SetAdditionalOutput(int value, float4 color, int3 pixel_coord) {{}}\n");
  }
  else
  {
    for (std::size_t i = 0; i < pass.m_additional_outputs.size(); i++)
    {
      shader_source.Write("IMAGE_BINDING(rgba8, {0}) uniform writeonly image2DArray  "
                          "additional_output_image{0};\n",
                          i + 1);
    }

    shader_source.Write("void SetAdditionalOutput(int value, float4 color, int3 pixel_coord) {{\n");
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
      shader_source.Write("\t\timageStore(additional_output_image{0}, pixel_coord, color);\n",
                          i + 1);
      shader_source.Write("\t}}\n");
    }
    shader_source.Write("}}\n\n");
  }

  shader_source.Write(R"(
#define GROUP_MEMORY_BARRIER_WITH_SYNC memoryBarrierShared(); barrier();
#define GROUP_SHARED shared

// Wrappers for sampling functions.
float4 SampleInputLocation(int value, float2 location) {{ return texture(samp[value], float3(location, float(u_layer))); }}

#define SampleInputLocationOffset(value, location, offset) (textureOffset(samp[value], float3(location.xy, float(u_layer)), offset))

float4 SampleInputLodLocation(int value, float lod, float2 location) {{ return textureLod(samp[value], float3(location, float(u_layer)), lod); }}
#define SampleInputLodLocationOffset(value, lod, location, offset) (textureLodOffset(samp[value], float3(location.xy, float(u_layer)), lod, offset))

void SetOutput(float4 color, int3 pixel_coord) {{ imageStore(output_image, pixel_coord, color); }}

ivec3 SampleInputSize(int value, int lod) {{ return textureSize(samp[value], lod); }}

float4 gather_red(int value, float3 location)
{{
  float4 result;
  result.a = SampleInputLocation(value, location.xy).r;
  result.b = SampleInputLocationOffset(value, location.xy, int2(1, 0)).r;
  result.r = SampleInputLocationOffset(value, location.xy, int2(0, 1)).r;
  result.g = SampleInputLocationOffset(value, location.xy, int2(1, 1)).r;
  return result;
}}

float4 gather_green(int value, float3 location)
{{
  float4 result;
  result.a = SampleInputLocation(value, location.xy).g;
  result.b = SampleInputLocationOffset(value, location.xy, int2(1, 0)).g;
  result.r = SampleInputLocationOffset(value, location.xy, int2(0, 1)).g;
  result.g = SampleInputLocationOffset(value, location.xy, int2(1, 1)).g;
  return result;
}}

float4 gather_blue(int value, float3 location)
{{
  float4 result;
  result.a = SampleInputLocation(value, location.xy).b;
  result.b = SampleInputLocationOffset(value, location.xy, int2(1, 0)).b;
  result.r = SampleInputLocationOffset(value, location.xy, int2(0, 1)).b;
  result.g = SampleInputLocationOffset(value, location.xy, int2(1, 1)).b;
  return result;
}}

float4 gather_alpha(int value, float3 location)
{{
  float4 result;
  result.a = SampleInputLocation(value, location.xy).a;
  result.b = SampleInputLocationOffset(value, location.xy, int2(1, 0)).a;
  result.r = SampleInputLocationOffset(value, location.xy, int2(0, 1)).a;
  result.g = SampleInputLocationOffset(value, location.xy, int2(1, 1)).a;
  return result;
}}
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
float ToLinearDepthInternal(float depth)
{{
  // TODO: The depth values provided by the projection matrix
  // produce invalid results, need to determine what the correct
  // values are
  //float NearZ = z_depth_near;
  //float FarZ = z_depth_far;

  // For now just hardcode our near/far planes
  float NearZ = 0.001f;
	float FarZ = 1.0f;
  const float A = (1.0f - (FarZ / NearZ)) / 2.0f;
  const float B = (1.0f + (FarZ / NearZ)) / 2.0f;
  return 1.0f / (A * depth + B);
  //return z_depth_linear_mul / (z_depth_linear_add - depth);
}}

float ToLinearDepth(float raw_depth)
{{
  return ToLinearDepthInternal(DEPTH_VALUE(raw_depth));
}}

float2 GetResolution() {{ return prev_resolution.xy; }}
float2 GetInvResolution() {{ return prev_resolution.zw; }}
float2 GetWindowResolution() {{  return window_resolution.xy; }}
float2 GetInvWindowResolution() {{ return window_resolution.zw; }}

float2 GetPrevResolution() {{ return prev_resolution.xy; }}
float2 GetInvPrevResolution() {{ return prev_resolution.zw; }}
float2 GetPrevRectOrigin() {{ return prev_rect.xy; }}
float2 GetPrevRectSize() {{ return prev_rect.zw; }}

float2 GetSrcResolution() {{ return src_resolution.xy; }}
float2 GetInvSrcResolution() {{ return src_resolution.zw; }}
float2 GetSrcRectOrigin() {{ return src_rect.xy; }}
float2 GetSrcRectSize() {{ return src_rect.zw; }}

float GetLayer() {{ return u_layer; }}
float GetTime() {{ return u_time; }}

#define GetOption(x) (x)
#define OptionEnabled(x) (x)

)");
}

void ComputeShader::ShaderFooter(ShaderCode& shader_source,
                                 const ShaderConfigPass& config_pass) const
{
  shader_source.Write("#undef main\n");
  shader_source.Write("layout(local_size_x = 8, local_size_y = 8) in;\n");

  shader_source.Write("void main()\n");
  shader_source.Write("{{\n");

  if (config_pass.m_entry_point == "main")
  {
    shader_source.Write(
        "\treal_main(gl_WorkGroupID, gl_LocalInvocationID, gl_GlobalInvocationID);\n");
  }
  else if (!config_pass.m_entry_point.empty())
  {
    shader_source.Write("\t{}(gl_WorkGroupID, gl_LocalInvocationID, gl_GlobalInvocationID);\n",
                        config_pass.m_entry_point);
  }

  shader_source.Write("}}\n");
}
}  // namespace VideoCommon::PE
