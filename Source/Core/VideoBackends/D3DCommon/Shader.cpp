// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3DCommon/Shader.h"

#include <fstream>
#include <optional>
#include <string_view>

#include <fmt/format.h>
#include <wrl/client.h>
#include "disassemble.h"
#include "spirv_hlsl.hpp"

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/HRWrap.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#include "VideoCommon/Spirv.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
// Regarding the UBO bind points, we subtract one from the binding index because
// the OpenGL backend requires UBO #0 for non-block uniforms (at least on NV).
// This allows us to share the same shaders but use bind point #0 in the D3D
// backends. None of the specific shaders use UBOs, instead they use push
// constants, so when/if the GL backend moves to uniform blocks completely this
// subtraction can be removed.
constexpr std::string_view SHADER_HEADER = R"(
  // Target GLSL 4.5.
  #version 450 core
  #define ATTRIBUTE_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
  #define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
  #define UBO_BINDING(packing, x) layout(packing, binding = (x - 1))
  #define SAMPLER_BINDING(x) layout(binding = x)
  #define TEXEL_BUFFER_BINDING(x) layout(binding = x)
  #define SSBO_BINDING(x) layout(binding = (x + 2))
  #define VARYING_LOCATION(x) layout(location = x)
  #define FORCE_EARLY_Z layout(early_fragment_tests) in

  // hlsl to glsl function translation
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

  #define API_D3D 1
)";
constexpr std::string_view COMPUTE_SHADER_HEADER = R"(
  // Target GLSL 4.5.
  #version 450 core
  // All resources are packed into one descriptor set for compute.
  #define UBO_BINDING(packing, x) layout(packing, binding = (x - 1))
  #define SAMPLER_BINDING(x) layout(binding = x)
  #define TEXEL_BUFFER_BINDING(x) layout(binding = x)
  #define IMAGE_BINDING(format, x) layout(format, binding = x)

  // hlsl to glsl function translation
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

  #define API_D3D 1
)";

std::optional<std::string> GetHLSLFromSPIRV(SPIRV::CodeVector spv, D3D_FEATURE_LEVEL feature_level)
{
  spirv_cross::CompilerHLSL::Options options;
  switch (feature_level)
  {
  case D3D_FEATURE_LEVEL_10_0:
    options.shader_model = 40;
    break;
  case D3D_FEATURE_LEVEL_10_1:
    options.shader_model = 41;
    break;
  default:
    options.shader_model = 50;
    break;
  };

  spirv_cross::CompilerHLSL compiler(std::move(spv));
  compiler.set_hlsl_options(options);

  return compiler.compile();
}

std::optional<SPIRV::CodeVector> GetSpirv(ShaderStage stage, std::string_view source)
{
  switch (stage)
  {
  case ShaderStage::Vertex:
  {
    const auto full_source = fmt::format("{}{}", SHADER_HEADER, source);
    return SPIRV::CompileVertexShader(full_source, APIType::D3D, glslang::EShTargetSpv_1_0);
  }

  case ShaderStage::Geometry:
  {
    // Spirv cross does not currently support hlsl geometry shaders
    return std::nullopt;
  }

  case ShaderStage::Pixel:
  {
    const auto full_source = fmt::format("{}{}", SHADER_HEADER, source);
    return SPIRV::CompileFragmentShader(full_source, APIType::D3D, glslang::EShTargetSpv_1_0);
  }

  case ShaderStage::Compute:
  {
    const auto full_source = fmt::format("{}{}", COMPUTE_SHADER_HEADER, source);
    return SPIRV::CompileComputeShader(full_source, APIType::D3D, glslang::EShTargetSpv_1_0);
  }
  };

  return std::nullopt;
}

std::optional<std::string> GetHLSL(D3D_FEATURE_LEVEL feature_level, ShaderStage stage,
                                   std::string_view source)
{
  if (stage == ShaderStage::Geometry)
  {
    return std::string{source};
  }
  else if (const auto spirv = GetSpirv(stage, source))
  {
    return GetHLSLFromSPIRV(std::move(*spirv), feature_level);
  }

  return std::nullopt;
}
}  // namespace

namespace D3DCommon
{
Shader::Shader(ShaderStage stage, BinaryData bytecode)
    : AbstractShader(stage), m_bytecode(std::move(bytecode))
{
}

Shader::~Shader() = default;

AbstractShader::BinaryData Shader::GetBinary() const
{
  return m_bytecode;
}

static const char* GetCompileTarget(D3D_FEATURE_LEVEL feature_level, ShaderStage stage)
{
  switch (stage)
  {
  case ShaderStage::Vertex:
  {
    switch (feature_level)
    {
    case D3D_FEATURE_LEVEL_10_0:
      return "vs_4_0";
    case D3D_FEATURE_LEVEL_10_1:
      return "vs_4_1";
    default:
      return "vs_5_0";
    }
  }

  case ShaderStage::Geometry:
  {
    switch (feature_level)
    {
    case D3D_FEATURE_LEVEL_10_0:
      return "gs_4_0";
    case D3D_FEATURE_LEVEL_10_1:
      return "gs_4_1";
    default:
      return "gs_5_0";
    }
  }

  case ShaderStage::Pixel:
  {
    switch (feature_level)
    {
    case D3D_FEATURE_LEVEL_10_0:
      return "ps_4_0";
    case D3D_FEATURE_LEVEL_10_1:
      return "ps_4_1";
    default:
      return "ps_5_0";
    }
  }

  case ShaderStage::Compute:
  {
    switch (feature_level)
    {
    case D3D_FEATURE_LEVEL_10_0:
    case D3D_FEATURE_LEVEL_10_1:
      return "";

    default:
      return "cs_5_0";
    }
  }

  default:
    return "";
  }
}

std::optional<Shader::BinaryData> Shader::CompileShader(D3D_FEATURE_LEVEL feature_level,
                                                        ShaderStage stage, std::string_view source)
{
  const auto hlsl = GetHLSL(feature_level, stage, source);
  if (!hlsl)
    return std::nullopt;

  static constexpr D3D_SHADER_MACRO macros[] = {{"API_D3D", "1"}, {nullptr, nullptr}};
  const UINT flags = g_ActiveConfig.bEnableValidationLayer ?
                         (D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION) :
                         (D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION);
  const char* target = GetCompileTarget(feature_level, stage);

  Microsoft::WRL::ComPtr<ID3DBlob> code;
  Microsoft::WRL::ComPtr<ID3DBlob> errors;
  HRESULT hr = d3d_compile(hlsl->data(), hlsl->size(), nullptr, macros, nullptr, "main", target,
                           flags, 0, &code, &errors);
  if (FAILED(hr))
  {
    static int num_failures = 0;
    std::string filename = VideoBackendBase::BadShaderFilename(target, num_failures++);
    std::ofstream file;
    File::OpenFStream(file, filename, std::ios_base::out);
    file.write(hlsl->data(), hlsl->size());
    file << "\n";
    file.write(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
    file << "\n";
    file << "Dolphin Version: " + Common::GetScmRevStr() + "\n";
    file << "Video Backend: " + g_video_backend->GetDisplayName();

    if (const auto spirv = GetSpirv(stage, source))
    {
      file << "\nOriginal Source: \n";
      file << source << std::endl;
      file << "SPIRV: \n";
      spv::Disassemble(file, *spirv);
    }
    file.close();

    PanicAlertFmt("Failed to compile {}: {}\nDebug info ({}):\n{}", filename, Common::HRWrap(hr),
                  target, static_cast<const char*>(errors->GetBufferPointer()));
    return std::nullopt;
  }

  if (errors && errors->GetBufferSize() > 0)
  {
    WARN_LOG_FMT(VIDEO, "{} compilation succeeded with warnings:\n{}", target,
                 static_cast<const char*>(errors->GetBufferPointer()));
  }

  return CreateByteCode(code->GetBufferPointer(), code->GetBufferSize());
}

AbstractShader::BinaryData Shader::CreateByteCode(const void* data, size_t length)
{
  const auto* const begin = static_cast<const u8*>(data);
  const auto* const end = begin + length;

  return {begin, end};
}

}  // namespace D3DCommon
