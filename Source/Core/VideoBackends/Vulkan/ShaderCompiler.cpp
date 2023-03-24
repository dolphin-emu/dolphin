// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/ShaderCompiler.h"

#include <cstddef>
#include <string>

#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/Spirv.h"

namespace Vulkan::ShaderCompiler
{
// Regarding the UBO bind points, we subtract one from the binding index because
// the OpenGL backend requires UBO #0 for non-block uniforms (at least on NV).
// This allows us to share the same shaders but use bind point #0 in the Vulkan
// backend. None of the Vulkan-specific shaders use UBOs, instead they use push
// constants, so when/if the GL backend moves to uniform blocks completely this
// subtraction can be removed.
static const char SHADER_HEADER[] = R"(
  // Target GLSL 4.5.
  #version 450 core
  #define ATTRIBUTE_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
  #define UBO_BINDING(packing, x) layout(packing, set = 0, binding = (x - 1))
  #define SAMPLER_BINDING(x) layout(set = 1, binding = x)
  #define TEXEL_BUFFER_BINDING(x) layout(set = 1, binding = (x + 8))
  #define SSBO_BINDING(x) layout(std430, set = 2, binding = x)
  #define INPUT_ATTACHMENT_BINDING(x, y, z) layout(set = x, binding = y, input_attachment_index = z)
  #define VARYING_LOCATION(x) layout(location = x)
  #define FORCE_EARLY_Z layout(early_fragment_tests) in

  // Metal framebuffer fetch helpers.
  #define FB_FETCH_VALUE subpassLoad(in_ocol0)

  // hlsl to glsl function translation
  #define API_VULKAN 1
  #define float2 vec2
  #define float3 vec3
  #define float4 vec4
  #define uint2 uvec2
  #define uint3 uvec3
  #define uint4 uvec4
  #define int2 ivec2
  #define int3 ivec3
  #define int4 ivec4
  #define frac fract
  #define lerp mix

  // These were changed in Vulkan
  #define gl_VertexID gl_VertexIndex
  #define gl_InstanceID gl_InstanceIndex
)";
static const char COMPUTE_SHADER_HEADER[] = R"(
  // Target GLSL 4.5.
  #version 450 core
  // All resources are packed into one descriptor set for compute.
  #define UBO_BINDING(packing, x) layout(packing, set = 0, binding = (x - 1))
  #define SAMPLER_BINDING(x) layout(set = 0, binding = (1 + x))
  #define TEXEL_BUFFER_BINDING(x) layout(set = 0, binding = (3 + x))
  #define IMAGE_BINDING(format, x) layout(format, set = 0, binding = (5 + x))

  // hlsl to glsl function translation
  #define API_VULKAN 1
  #define float2 vec2
  #define float3 vec3
  #define float4 vec4
  #define uint2 uvec2
  #define uint3 uvec3
  #define uint4 uvec4
  #define int2 ivec2
  #define int3 ivec3
  #define int4 ivec4
  #define frac fract
  #define lerp mix
)";
static const char SUBGROUP_HELPER_HEADER[] = R"(
  #extension GL_KHR_shader_subgroup_basic : enable
  #extension GL_KHR_shader_subgroup_arithmetic : enable
  #extension GL_KHR_shader_subgroup_ballot : enable
  #extension GL_KHR_shader_subgroup_shuffle : enable

  #define SUPPORTS_SUBGROUP_REDUCTION 1
  #define IS_HELPER_INVOCATION gl_HelperInvocation
  #define IS_FIRST_ACTIVE_INVOCATION (subgroupElect())
  #define SUBGROUP_MIN(value) value = subgroupMin(value)
  #define SUBGROUP_MAX(value) value = subgroupMax(value)
)";

static std::string GetShaderCode(std::string_view source, std::string_view header)
{
  std::string full_source_code;
  if (!header.empty())
  {
    constexpr size_t subgroup_helper_header_length = std::size(SUBGROUP_HELPER_HEADER) - 1;
    full_source_code.reserve(header.size() + subgroup_helper_header_length + source.size());
    full_source_code.append(header);
    if (g_vulkan_context->SupportsShaderSubgroupOperations())
      full_source_code.append(SUBGROUP_HELPER_HEADER, subgroup_helper_header_length);
    full_source_code.append(source);
  }

  return full_source_code;
}

static glslang::EShTargetLanguageVersion GetLanguageVersion()
{
  // Sub-group operations require Vulkan 1.1 and SPIR-V 1.3.
  if (g_vulkan_context->SupportsShaderSubgroupOperations())
    return glslang::EShTargetSpv_1_3;

  return glslang::EShTargetSpv_1_0;
}

std::optional<SPIRVCodeVector> CompileVertexShader(std::string_view source_code)
{
  return SPIRV::CompileVertexShader(GetShaderCode(source_code, SHADER_HEADER), APIType::Vulkan,
                                    GetLanguageVersion());
}

std::optional<SPIRVCodeVector> CompileGeometryShader(std::string_view source_code)
{
  return SPIRV::CompileGeometryShader(GetShaderCode(source_code, SHADER_HEADER), APIType::Vulkan,
                                      GetLanguageVersion());
}

std::optional<SPIRVCodeVector> CompileFragmentShader(std::string_view source_code)
{
  return SPIRV::CompileFragmentShader(GetShaderCode(source_code, SHADER_HEADER), APIType::Vulkan,
                                      GetLanguageVersion());
}

std::optional<SPIRVCodeVector> CompileComputeShader(std::string_view source_code)
{
  return SPIRV::CompileComputeShader(GetShaderCode(source_code, COMPUTE_SHADER_HEADER),
                                     APIType::Vulkan, GetLanguageVersion());
}
}  // namespace Vulkan::ShaderCompiler
