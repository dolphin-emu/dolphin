// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/ShaderCompiler.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>

// glslang includes
#include "GlslangToSpv.h"
#include "ShaderLang.h"
#include "disassemble.h"

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
namespace ShaderCompiler
{
// Registers itself for cleanup via atexit
bool InitializeGlslang();

// Resource limits used when compiling shaders
static const TBuiltInResource* GetCompilerResourceLimits();

// Compile a shader to SPIR-V via glslang
static bool CompileShaderToSPV(SPIRVCodeVector* out_code, EShLanguage stage,
                               const char* stage_filename, const char* source_code,
                               size_t source_code_length, const char* header, size_t header_length);

// Copy GLSL source code to a SPIRVCodeVector, for use with VK_NV_glsl_shader.
static void CopyGLSLToSPVVector(SPIRVCodeVector* out_code, const char* stage_filename,
                                const char* source_code, size_t source_code_length,
                                const char* header, size_t header_length);

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
  #define SSBO_BINDING(x) layout(set = 2, binding = x)
  #define TEXEL_BUFFER_BINDING(x) layout(set = 2, binding = x)
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

  // These were changed in Vulkan
  #define gl_VertexID gl_VertexIndex
  #define gl_InstanceID gl_InstanceIndex
)";
static const char COMPUTE_SHADER_HEADER[] = R"(
  // Target GLSL 4.5.
  #version 450 core
  // All resources are packed into one descriptor set for compute.
  #define UBO_BINDING(packing, x) layout(packing, set = 0, binding = (0 + x))
  #define SAMPLER_BINDING(x) layout(set = 0, binding = (1 + x))
  #define TEXEL_BUFFER_BINDING(x) layout(set = 0, binding = (5 + x))
  #define IMAGE_BINDING(format, x) layout(format, set = 0, binding = (7 + x))

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
)";

bool CompileShaderToSPV(SPIRVCodeVector* out_code, EShLanguage stage, const char* stage_filename,
                        const char* source_code, size_t source_code_length, const char* header,
                        size_t header_length)
{
  if (!InitializeGlslang())
    return false;

  std::unique_ptr<glslang::TShader> shader = std::make_unique<glslang::TShader>(stage);
  std::unique_ptr<glslang::TProgram> program;
  glslang::TShader::ForbidInclude includer;
  EProfile profile = ECoreProfile;
  EShMessages messages =
      static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
  int default_version = 450;

  std::string full_source_code;
  const char* pass_source_code = source_code;
  int pass_source_code_length = static_cast<int>(source_code_length);
  if (header_length > 0)
  {
    full_source_code.reserve(header_length + source_code_length);
    full_source_code.append(header, header_length);
    full_source_code.append(source_code, source_code_length);
    pass_source_code = full_source_code.c_str();
    pass_source_code_length = static_cast<int>(full_source_code.length());
  }

  shader->setStringsWithLengths(&pass_source_code, &pass_source_code_length, 1);

  auto DumpBadShader = [&](const char* msg) {
    static int counter = 0;
    std::string filename = StringFromFormat(
        "%sbad_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), stage_filename, counter++);

    std::ofstream stream;
    File::OpenFStream(stream, filename, std::ios_base::out);
    if (stream.good())
    {
      stream << full_source_code << std::endl;
      stream << msg << std::endl;
      stream << "Shader Info Log:" << std::endl;
      stream << shader->getInfoLog() << std::endl;
      stream << shader->getInfoDebugLog() << std::endl;
      if (program)
      {
        stream << "Program Info Log:" << std::endl;
        stream << program->getInfoLog() << std::endl;
        stream << program->getInfoDebugLog() << std::endl;
      }
    }

    PanicAlert("%s (written to %s)", msg, filename.c_str());
  };

  if (!shader->parse(GetCompilerResourceLimits(), default_version, profile, false, true, messages,
                     includer))
  {
    DumpBadShader("Failed to parse shader");
    return false;
  }

  // Even though there's only a single shader, we still need to link it to generate SPV
  program = std::make_unique<glslang::TProgram>();
  program->addShader(shader.get());
  if (!program->link(messages))
  {
    DumpBadShader("Failed to link program");
    return false;
  }

  glslang::TIntermediate* intermediate = program->getIntermediate(stage);
  if (!intermediate)
  {
    DumpBadShader("Failed to generate SPIR-V");
    return false;
  }

  spv::SpvBuildLogger logger;
  glslang::GlslangToSpv(*intermediate, *out_code, &logger);

  // Write out messages
  // Temporary: skip if it contains "Warning, version 450 is not yet complete; most version-specific
  // features are present, but some are missing."
  if (strlen(shader->getInfoLog()) > 108)
    WARN_LOG(VIDEO, "Shader info log: %s", shader->getInfoLog());
  if (strlen(shader->getInfoDebugLog()) > 0)
    WARN_LOG(VIDEO, "Shader debug info log: %s", shader->getInfoDebugLog());
  if (strlen(program->getInfoLog()) > 25)
    WARN_LOG(VIDEO, "Program info log: %s", program->getInfoLog());
  if (strlen(program->getInfoDebugLog()) > 0)
    WARN_LOG(VIDEO, "Program debug info log: %s", program->getInfoDebugLog());
  std::string spv_messages = logger.getAllMessages();
  if (!spv_messages.empty())
    WARN_LOG(VIDEO, "SPIR-V conversion messages: %s", spv_messages.c_str());

  // Dump source code of shaders out to file if enabled.
  if (g_ActiveConfig.iLog & CONF_SAVESHADERS)
  {
    static int counter = 0;
    std::string filename = StringFromFormat("%s%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(),
                                            stage_filename, counter++);

    std::ofstream stream;
    File::OpenFStream(stream, filename, std::ios_base::out);
    if (stream.good())
    {
      stream << full_source_code << std::endl;
      stream << "Shader Info Log:" << std::endl;
      stream << shader->getInfoLog() << std::endl;
      stream << shader->getInfoDebugLog() << std::endl;
      stream << "Program Info Log:" << std::endl;
      stream << program->getInfoLog() << std::endl;
      stream << program->getInfoDebugLog() << std::endl;
      stream << "SPIR-V conversion messages: " << std::endl;
      stream << spv_messages;
      stream << "SPIR-V:" << std::endl;
      spv::Disassemble(stream, *out_code);
    }
  }

  return true;
}

void CopyGLSLToSPVVector(SPIRVCodeVector* out_code, const char* stage_filename,
                         const char* source_code, size_t source_code_length, const char* header,
                         size_t header_length)
{
  std::string full_source_code;
  if (header_length > 0)
  {
    full_source_code.reserve(header_length + source_code_length);
    full_source_code.append(header, header_length);
    full_source_code.append(source_code, source_code_length);
  }
  else
  {
    full_source_code.append(source_code, source_code_length);
  }

  if (g_ActiveConfig.iLog & CONF_SAVESHADERS)
  {
    static int counter = 0;
    std::string filename = StringFromFormat("%s%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(),
                                            stage_filename, counter++);

    std::ofstream stream;
    File::OpenFStream(stream, filename, std::ios_base::out);
    if (stream.good())
      stream << full_source_code << std::endl;
  }

  size_t padding = full_source_code.size() % 4;
  if (padding != 0)
    full_source_code.append(4 - padding, '\n');

  out_code->resize(full_source_code.size() / 4);
  std::memcpy(out_code->data(), full_source_code.c_str(), full_source_code.size());
}

bool InitializeGlslang()
{
  static bool glslang_initialized = false;
  if (glslang_initialized)
    return true;

  if (!glslang::InitializeProcess())
  {
    PanicAlert("Failed to initialize glslang shader compiler");
    return false;
  }

  std::atexit([]() { glslang::FinalizeProcess(); });

  glslang_initialized = true;
  return true;
}

const TBuiltInResource* GetCompilerResourceLimits()
{
  static const TBuiltInResource limits = {/* .MaxLights = */ 32,
                                          /* .MaxClipPlanes = */ 6,
                                          /* .MaxTextureUnits = */ 32,
                                          /* .MaxTextureCoords = */ 32,
                                          /* .MaxVertexAttribs = */ 64,
                                          /* .MaxVertexUniformComponents = */ 4096,
                                          /* .MaxVaryingFloats = */ 64,
                                          /* .MaxVertexTextureImageUnits = */ 32,
                                          /* .MaxCombinedTextureImageUnits = */ 80,
                                          /* .MaxTextureImageUnits = */ 32,
                                          /* .MaxFragmentUniformComponents = */ 4096,
                                          /* .MaxDrawBuffers = */ 32,
                                          /* .MaxVertexUniformVectors = */ 128,
                                          /* .MaxVaryingVectors = */ 8,
                                          /* .MaxFragmentUniformVectors = */ 16,
                                          /* .MaxVertexOutputVectors = */ 16,
                                          /* .MaxFragmentInputVectors = */ 15,
                                          /* .MinProgramTexelOffset = */ -8,
                                          /* .MaxProgramTexelOffset = */ 7,
                                          /* .MaxClipDistances = */ 8,
                                          /* .MaxComputeWorkGroupCountX = */ 65535,
                                          /* .MaxComputeWorkGroupCountY = */ 65535,
                                          /* .MaxComputeWorkGroupCountZ = */ 65535,
                                          /* .MaxComputeWorkGroupSizeX = */ 1024,
                                          /* .MaxComputeWorkGroupSizeY = */ 1024,
                                          /* .MaxComputeWorkGroupSizeZ = */ 64,
                                          /* .MaxComputeUniformComponents = */ 1024,
                                          /* .MaxComputeTextureImageUnits = */ 16,
                                          /* .MaxComputeImageUniforms = */ 8,
                                          /* .MaxComputeAtomicCounters = */ 8,
                                          /* .MaxComputeAtomicCounterBuffers = */ 1,
                                          /* .MaxVaryingComponents = */ 60,
                                          /* .MaxVertexOutputComponents = */ 64,
                                          /* .MaxGeometryInputComponents = */ 64,
                                          /* .MaxGeometryOutputComponents = */ 128,
                                          /* .MaxFragmentInputComponents = */ 128,
                                          /* .MaxImageUnits = */ 8,
                                          /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
                                          /* .MaxCombinedShaderOutputResources = */ 8,
                                          /* .MaxImageSamples = */ 0,
                                          /* .MaxVertexImageUniforms = */ 0,
                                          /* .MaxTessControlImageUniforms = */ 0,
                                          /* .MaxTessEvaluationImageUniforms = */ 0,
                                          /* .MaxGeometryImageUniforms = */ 0,
                                          /* .MaxFragmentImageUniforms = */ 8,
                                          /* .MaxCombinedImageUniforms = */ 8,
                                          /* .MaxGeometryTextureImageUnits = */ 16,
                                          /* .MaxGeometryOutputVertices = */ 256,
                                          /* .MaxGeometryTotalOutputComponents = */ 1024,
                                          /* .MaxGeometryUniformComponents = */ 1024,
                                          /* .MaxGeometryVaryingComponents = */ 64,
                                          /* .MaxTessControlInputComponents = */ 128,
                                          /* .MaxTessControlOutputComponents = */ 128,
                                          /* .MaxTessControlTextureImageUnits = */ 16,
                                          /* .MaxTessControlUniformComponents = */ 1024,
                                          /* .MaxTessControlTotalOutputComponents = */ 4096,
                                          /* .MaxTessEvaluationInputComponents = */ 128,
                                          /* .MaxTessEvaluationOutputComponents = */ 128,
                                          /* .MaxTessEvaluationTextureImageUnits = */ 16,
                                          /* .MaxTessEvaluationUniformComponents = */ 1024,
                                          /* .MaxTessPatchComponents = */ 120,
                                          /* .MaxPatchVertices = */ 32,
                                          /* .MaxTessGenLevel = */ 64,
                                          /* .MaxViewports = */ 16,
                                          /* .MaxVertexAtomicCounters = */ 0,
                                          /* .MaxTessControlAtomicCounters = */ 0,
                                          /* .MaxTessEvaluationAtomicCounters = */ 0,
                                          /* .MaxGeometryAtomicCounters = */ 0,
                                          /* .MaxFragmentAtomicCounters = */ 8,
                                          /* .MaxCombinedAtomicCounters = */ 8,
                                          /* .MaxAtomicCounterBindings = */ 1,
                                          /* .MaxVertexAtomicCounterBuffers = */ 0,
                                          /* .MaxTessControlAtomicCounterBuffers = */ 0,
                                          /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
                                          /* .MaxGeometryAtomicCounterBuffers = */ 0,
                                          /* .MaxFragmentAtomicCounterBuffers = */ 1,
                                          /* .MaxCombinedAtomicCounterBuffers = */ 1,
                                          /* .MaxAtomicCounterBufferSize = */ 16384,
                                          /* .MaxTransformFeedbackBuffers = */ 4,
                                          /* .MaxTransformFeedbackInterleavedComponents = */ 64,
                                          /* .MaxCullDistances = */ 8,
                                          /* .MaxCombinedClipAndCullDistances = */ 8,
                                          /* .MaxSamples = */ 4,
                                          /* .limits = */ {
                                              /* .nonInductiveForLoops = */ 1,
                                              /* .whileLoops = */ 1,
                                              /* .doWhileLoops = */ 1,
                                              /* .generalUniformIndexing = */ 1,
                                              /* .generalAttributeMatrixVectorIndexing = */ 1,
                                              /* .generalVaryingIndexing = */ 1,
                                              /* .generalSamplerIndexing = */ 1,
                                              /* .generalVariableIndexing = */ 1,
                                              /* .generalConstantMatrixVectorIndexing = */ 1,
                                          }};

  return &limits;
}

bool CompileVertexShader(SPIRVCodeVector* out_code, const char* source_code,
                         size_t source_code_length)
{
  if (g_vulkan_context->SupportsNVGLSLExtension())
  {
    CopyGLSLToSPVVector(out_code, "vs", source_code, source_code_length, SHADER_HEADER,
                        sizeof(SHADER_HEADER) - 1);
    return true;
  }

  return CompileShaderToSPV(out_code, EShLangVertex, "vs", source_code, source_code_length,
                            SHADER_HEADER, sizeof(SHADER_HEADER) - 1);
}

bool CompileGeometryShader(SPIRVCodeVector* out_code, const char* source_code,
                           size_t source_code_length)
{
  if (g_vulkan_context->SupportsNVGLSLExtension())
  {
    CopyGLSLToSPVVector(out_code, "gs", source_code, source_code_length, SHADER_HEADER,
                        sizeof(SHADER_HEADER) - 1);
    return true;
  }

  return CompileShaderToSPV(out_code, EShLangGeometry, "gs", source_code, source_code_length,
                            SHADER_HEADER, sizeof(SHADER_HEADER) - 1);
}

bool CompileFragmentShader(SPIRVCodeVector* out_code, const char* source_code,
                           size_t source_code_length)
{
  if (g_vulkan_context->SupportsNVGLSLExtension())
  {
    CopyGLSLToSPVVector(out_code, "ps", source_code, source_code_length, SHADER_HEADER,
                        sizeof(SHADER_HEADER) - 1);
    return true;
  }

  return CompileShaderToSPV(out_code, EShLangFragment, "ps", source_code, source_code_length,
                            SHADER_HEADER, sizeof(SHADER_HEADER) - 1);
}

bool CompileComputeShader(SPIRVCodeVector* out_code, const char* source_code,
                          size_t source_code_length)
{
  if (g_vulkan_context->SupportsNVGLSLExtension())
  {
    CopyGLSLToSPVVector(out_code, "cs", source_code, source_code_length, COMPUTE_SHADER_HEADER,
                        sizeof(COMPUTE_SHADER_HEADER) - 1);
    return true;
  }

  return CompileShaderToSPV(out_code, EShLangCompute, "cs", source_code, source_code_length,
                            COMPUTE_SHADER_HEADER, sizeof(COMPUTE_SHADER_HEADER) - 1);
}

}  // namespace ShaderCompiler
}  // namespace Vulkan
