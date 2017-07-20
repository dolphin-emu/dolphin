// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/PostProcessing.h"
#include <sstream>

#include "Common/Assert.h"
#include "Common/StringUtil.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
VulkanPostProcessing::~VulkanPostProcessing()
{
  if (m_default_fragment_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_default_fragment_shader, nullptr);
  if (m_fragment_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_fragment_shader, nullptr);
}

bool VulkanPostProcessing::Initialize(const Texture2D* font_texture)
{
  m_font_texture = font_texture;
  if (!CompileDefaultShader())
    return false;

  RecompileShader();
  return true;
}

void VulkanPostProcessing::BlitFromTexture(const TargetRectangle& dst, const TargetRectangle& src,
                                           const Texture2D* src_tex, int src_layer,
                                           VkRenderPass render_pass)
{
  // If the source layer is negative we simply copy all available layers.
  VkShaderModule geometry_shader =
      src_layer < 0 ? g_shader_cache->GetPassthroughGeometryShader() : VK_NULL_HANDLE;
  VkShaderModule fragment_shader =
      m_fragment_shader != VK_NULL_HANDLE ? m_fragment_shader : m_default_fragment_shader;
  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD), render_pass,
                         g_shader_cache->GetPassthroughVertexShader(), geometry_shader,
                         fragment_shader);

  // Source is always bound.
  draw.SetPSSampler(0, src_tex->GetView(), g_object_cache->GetLinearSampler());

  // No need to allocate uniforms for the default shader.
  // The config will also still contain the invalid shader at this point.
  if (fragment_shader != m_default_fragment_shader)
  {
    size_t uniforms_size = CalculateUniformsSize();
    u8* uniforms = draw.AllocatePSUniforms(uniforms_size);
    FillUniformBuffer(uniforms, src, src_tex, src_layer);
    draw.CommitPSUniforms(uniforms_size);
    draw.SetPSSampler(1, m_font_texture->GetView(), g_object_cache->GetLinearSampler());
  }

  draw.DrawQuad(dst.left, dst.top, dst.GetWidth(), dst.GetHeight(), src.left, src.top, src_layer,
                src.GetWidth(), src.GetHeight(), static_cast<int>(src_tex->GetWidth()),
                static_cast<int>(src_tex->GetHeight()));
}

struct BuiltinUniforms
{
  float resolution[4];
  float src_rect[4];
  u32 time;
  u32 unused[3];
};

size_t VulkanPostProcessing::CalculateUniformsSize() const
{
  // Allocate a vec4 for each uniform to simplify allocation.
  return sizeof(BuiltinUniforms) + m_config.GetOptions().size() * sizeof(float) * 4;
}

void VulkanPostProcessing::FillUniformBuffer(u8* buf, const TargetRectangle& src,
                                             const Texture2D* src_tex, int src_layer)
{
  float src_width_float = static_cast<float>(src_tex->GetWidth());
  float src_height_float = static_cast<float>(src_tex->GetHeight());
  BuiltinUniforms builtin_uniforms = {
      {src_width_float, src_height_float, 1.0f / src_width_float, 1.0f / src_height_float},
      {static_cast<float>(src.left) / src_width_float,
       static_cast<float>(src.top) / src_height_float,
       static_cast<float>(src.GetWidth()) / src_width_float,
       static_cast<float>(src.GetHeight()) / src_height_float},
      static_cast<u32>(m_timer.GetTimeElapsed())};

  std::memcpy(buf, &builtin_uniforms, sizeof(builtin_uniforms));
  buf += sizeof(builtin_uniforms);

  for (const auto& it : m_config.GetOptions())
  {
    union
    {
      u32 as_bool[4];
      s32 as_int[4];
      float as_float[4];
    } value = {};

    switch (it.second.m_type)
    {
    case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL:
      value.as_bool[0] = it.second.m_bool_value ? 1 : 0;
      break;

    case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER:
      _assert_(it.second.m_integer_values.size() < 4);
      std::copy_n(it.second.m_integer_values.begin(), it.second.m_integer_values.size(),
                  value.as_int);
      break;

    case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT:
      _assert_(it.second.m_float_values.size() < 4);
      std::copy_n(it.second.m_float_values.begin(), it.second.m_float_values.size(),
                  value.as_float);
      break;
    }

    std::memcpy(buf, &value, sizeof(value));
    buf += sizeof(value);
  }
}

static const std::string DEFAULT_FRAGMENT_SHADER_SOURCE = R"(
  layout(set = 1, binding = 0) uniform sampler2DArray samp0;

  layout(location = 0) in float3 uv0;
  layout(location = 1) in float4 col0;
  layout(location = 0) out float4 ocol0;

  void main()
  {
    ocol0 = float4(texture(samp0, uv0).xyz, 1.0);
  }
)";

static const std::string POSTPROCESSING_SHADER_HEADER = R"(
  SAMPLER_BINDING(0) uniform sampler2DArray samp0;
  SAMPLER_BINDING(1) uniform sampler2DArray samp1;

  layout(location = 0) in float3 uv0;
  layout(location = 1) in float4 col0;
  layout(location = 0) out float4 ocol0;

  // Interfacing functions
  // The EFB may have a zero alpha value, which we don't want to write to the frame dump, so set it to one here.
  float4 Sample()
  {
    return float4(texture(samp0, uv0).xyz, 1.0);
  }

  float4 SampleLocation(float2 location)
  {
    return float4(texture(samp0, float3(location, uv0.z)).xyz, 1.0);
  }

  float4 SampleLayer(int layer)
  {
    return float4(texture(samp0, float3(uv0.xy, float(layer))).xyz, 1.0);
  }

  #define SampleOffset(offset) float4(textureOffset(samp0, uv0, offset).xyz, 1.0)

  float4 SampleFontLocation(float2 location)
  {
    return texture(samp1, float3(location, 0.0));
  }

  float2 GetResolution()
  {
    return options.resolution.xy;
  }

  float2 GetInvResolution()
  {
    return options.resolution.zw;
  }

  float2 GetCoordinates()
  {
    return uv0.xy;
  }

  uint GetTime()
  {
    return options.time;
  }

  void SetOutput(float4 color)
  {
    ocol0 = color;
  }

  #define GetOption(x) (options.x)
  #define OptionEnabled(x) (options.x != 0)

  // Workaround because there is no getter function for src rect/layer.
  float4 src_rect = options.src_rect;
  int layer = int(uv0.z);
)";

void VulkanPostProcessing::UpdateConfig()
{
  if (m_config.GetShader() == g_ActiveConfig.sPostProcessingShader)
    return;

  RecompileShader();
}

bool VulkanPostProcessing::CompileDefaultShader()
{
  m_default_fragment_shader = Util::CompileAndCreateFragmentShader(DEFAULT_FRAGMENT_SHADER_SOURCE);
  if (m_default_fragment_shader == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to compile default post-processing shader.");
    return false;
  }

  return true;
}

bool VulkanPostProcessing::RecompileShader()
{
  // As a driver can return the same new module pointer when destroying a shader and re-compiling,
  // we need to wipe out the pipeline cache, otherwise we risk using old pipelines with old shaders.
  // We can't just clear a single pipeline, because we don't know which render pass is going to be
  // used here either.
  if (m_fragment_shader != VK_NULL_HANDLE)
  {
    g_command_buffer_mgr->WaitForGPUIdle();
    g_shader_cache->ClearPipelineCache();
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_fragment_shader, nullptr);
    m_fragment_shader = VK_NULL_HANDLE;
  }

  // If post-processing is disabled, just use the default shader.
  // This way we don't need to allocate uniforms.
  if (g_ActiveConfig.sPostProcessingShader.empty())
    return true;

  // Generate GLSL and compile the new shader.
  std::string main_code = m_config.LoadShader();
  std::string options_code = GetGLSLUniformBlock();
  std::string code = options_code + POSTPROCESSING_SHADER_HEADER + main_code;
  m_fragment_shader = Util::CompileAndCreateFragmentShader(code);
  if (m_fragment_shader == VK_NULL_HANDLE)
  {
    // BlitFromTexture will use the default shader as a fallback.
    PanicAlert("Failed to compile post-processing shader %s", m_config.GetShader().c_str());
    return false;
  }

  return true;
}

std::string VulkanPostProcessing::GetGLSLUniformBlock() const
{
  std::stringstream ss;
  u32 unused_counter = 1;
  ss << "UBO_BINDING(std140, 1) uniform PSBlock {\n";

  // Builtin uniforms
  ss << "  float4 resolution;\n";
  ss << "  float4 src_rect;\n";
  ss << "  uint time;\n";
  for (u32 i = 0; i < 3; i++)
    ss << "  uint unused" << unused_counter++ << ";\n\n";

  // Custom options/uniforms
  for (const auto& it : m_config.GetOptions())
  {
    if (it.second.m_type ==
        PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL)
    {
      ss << StringFromFormat("  int %s;\n", it.first.c_str());
      for (u32 i = 0; i < 3; i++)
        ss << "  int unused" << unused_counter++ << ";\n";
    }
    else if (it.second.m_type ==
             PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
    {
      u32 count = static_cast<u32>(it.second.m_integer_values.size());
      if (count == 1)
        ss << StringFromFormat("  int %s;\n", it.first.c_str());
      else
        ss << StringFromFormat("  int%u %s;\n", count, it.first.c_str());

      for (u32 i = count; i < 4; i++)
        ss << "  int unused" << unused_counter++ << ";\n";
    }
    else if (it.second.m_type ==
             PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT)
    {
      u32 count = static_cast<u32>(it.second.m_float_values.size());
      if (count == 1)
        ss << StringFromFormat("  float %s;\n", it.first.c_str());
      else
        ss << StringFromFormat("  float%u %s;\n", count, it.first.c_str());

      for (u32 i = count; i < 4; i++)
        ss << "  float unused" << unused_counter++ << ";\n";
    }
  }

  ss << "} options;\n\n";

  return ss.str();
}

}  // namespace Vulkan
