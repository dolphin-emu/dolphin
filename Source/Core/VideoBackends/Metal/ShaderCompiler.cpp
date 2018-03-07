// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/ShaderCompiler.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>

// glslang includes
#include <GlslangToSpv.h>
#include <ShaderLang.h>
#include <disassemble.h>
#include <spirv_msl.hpp>

#include "Common/CommonFuncs.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/VideoConfig.h"

namespace Metal
{
namespace ShaderCompiler
{
// Registers itself for cleanup via atexit
bool InitializeGlslang();

// Resource limits used when compiling shaders
static const TBuiltInResource* GetCompilerResourceLimits();

// Compile a shader to SPIR-V via glslang
static bool CompileShaderToSPV(SPIRVCodeVector* out_code, ShaderStage stage,
                               const char* source_code, size_t source_code_length,
                               const char* header, size_t header_length);

static const char SHADER_HEADER[] = R"(
// Target GLSL 4.5.
#version 450 core
#define ATTRIBUTE_LOCATION(x) layout(location = x)
#define FRAGMENT_OUTPUT_LOCATION(x) layout(location = x)
#define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y) layout(location = x, index = y)
#define UBO_BINDING(packing, x) layout(packing, set = 0, binding = ((x) - 1))
#define SAMPLER_BINDING(x) layout(set = 1, binding = x)
#define SSBO_BINDING(x) layout(set = 2, binding = x)
#define TEXEL_BUFFER_BINDING(x) layout(set = 3, binding = x)
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

// MSL doesn't define sign() for integer types.
int sign(int x) { return x == 0 ? 0 : (x < 0 ? -1 : 1); }
int2 sign(int2 x) { return int2(sign(x.x), sign(x.y)); }
int3 sign(int3 x) { return int3(sign(x.x), sign(x.y), sign(x.z)); }
int4 sign(int4 x) { return int4(sign(x.x), sign(x.y), sign(x.z), sign(x.w)); }
)";

const char* GetStagePrefix(ShaderStage stage)
{
  switch (stage)
  {
  case ShaderStage::Vertex:
    return "vs";
  case ShaderStage::Pixel:
    return "ps";
  case ShaderStage::Compute:
    return "cs";
  default:
    return "";
  }
}

bool CompileShaderToSPV(SPIRVCodeVector* out_code, ShaderStage stage, const char* source_code,
                        size_t source_code_length, const char* header, size_t header_length)
{
  if (!InitializeGlslang())
    return false;

  EShLanguage lang = EShLangCount;
  switch (stage)
  {
  case ShaderStage::Vertex:
    lang = EShLangVertex;
    break;
  case ShaderStage::Pixel:
    lang = EShLangFragment;
    break;
  case ShaderStage::Compute:
    lang = EShLangCompute;
    break;
  default:
    return false;
  }

  std::unique_ptr<glslang::TShader> shader = std::make_unique<glslang::TShader>(lang);
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
    std::string filename =
        StringFromFormat("%sbad_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(),
                         GetStagePrefix(stage), counter++);

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

  glslang::TIntermediate* intermediate = program->getIntermediate(lang);
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
                                            GetStagePrefix(stage), counter++);

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
                                          /* .MaxGeometryInputComponents = */ 0,
                                          /* .MaxGeometryOutputComponents = */ 0,
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
                                          /* .MaxGeometryTextureImageUnits = */ 0,
                                          /* .MaxGeometryOutputVertices = */ 0,
                                          /* .MaxGeometryTotalOutputComponents = */ 0,
                                          /* .MaxGeometryUniformComponents = */ 0,
                                          /* .MaxGeometryVaryingComponents = */ 0,
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
                                          /* .limits = */
                                          {
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

bool CompileShaderToSPV(SPIRVCodeVector* out_code, ShaderStage stage, const char* source_code,
                        size_t source_code_length)
{
  return CompileShaderToSPV(out_code, stage, source_code, source_code_length, SHADER_HEADER,
                            sizeof(SHADER_HEADER) - 1);
}

bool TranslateSPVToMSL(std::string* out_msl, ShaderStage stage, const SPIRVCodeVector* in_spv)
{
  spirv_cross::MSLResourceBinding resource_bindings[] = {
      {spv::ExecutionModelVertex, 0, 1, FIRST_VERTEX_UBO_INDEX, 0, 0},       // vs/ubo
      {spv::ExecutionModelFragment, 0, 0, FIRST_PIXEL_UBO_INDEX, 0, 0},      // vs/ubo
      {spv::ExecutionModelFragment, 0, 1, FIRST_PIXEL_UBO_INDEX + 1, 0, 0},  // vs/ubo
      {spv::ExecutionModelFragment, 1, 0, 0, 0, 0},                          // ps/samp0
      {spv::ExecutionModelFragment, 1, 1, 0, 1, 1},                          // ps/samp1
      {spv::ExecutionModelFragment, 1, 2, 0, 2, 2},                          // ps/samp2
      {spv::ExecutionModelFragment, 1, 3, 0, 3, 3},                          // ps/samp3
      {spv::ExecutionModelFragment, 1, 4, 0, 4, 4},                          // ps/samp4
      {spv::ExecutionModelFragment, 1, 5, 0, 5, 5},                          // ps/samp5
      {spv::ExecutionModelFragment, 1, 6, 0, 6, 6},                          // ps/samp6
      {spv::ExecutionModelFragment, 1, 7, 0, 7, 7},                          // ps/samp7
      {spv::ExecutionModelFragment, 1, 8, 0, 8, 8},                          // ps/samp8
      {spv::ExecutionModelFragment, 1, 9, 0, 9, 9}                           // ps/samp9
  };

  spirv_cross::CompilerMSL compiler(in_spv->data(), in_spv->size(), nullptr, 0, resource_bindings,
                                    ArraySize(resource_bindings));
  spirv_cross::CompilerMSL::Options options;
  options.platform = spirv_cross::CompilerMSL::Options::macOS;
  options.set_msl_version(1, 3);
  compiler.set_options(options);

  *out_msl = compiler.compile();
  if (out_msl->empty())
    return false;

  if (g_ActiveConfig.iLog & CONF_SAVESHADERS)
  {
    static int counter = 0;
    std::string filename =
        StringFromFormat("%smsl_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(),
                         GetStagePrefix(stage), counter++);
    std::ofstream stream;
    File::OpenFStream(stream, filename, std::ios_base::out);
    if (stream.good())
      stream << *out_msl;
  }

  return true;
}

}  // namespace ShaderCompiler
}  // namespace Metal
