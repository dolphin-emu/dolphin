// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Spirv.h"

#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/SPIRV/disassemble.h>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
bool InitializeGlslang()
{
  static bool glslang_initialized = false;
  if (glslang_initialized)
    return true;

  if (!glslang::InitializeProcess())
  {
    PanicAlertFmt("Failed to initialize glslang shader compiler");
    return false;
  }

  std::atexit([] { glslang::FinalizeProcess(); });

  glslang_initialized = true;
  return true;
}

const TBuiltInResource* GetCompilerResourceLimits()
{
  static const TBuiltInResource default_resource = {
      /* .MaxLights = */ 32,
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
      /* .maxMeshOutputVerticesNV = */ 256,
      /* .maxMeshOutputPrimitivesNV = */ 512,
      /* .maxMeshWorkGroupSizeX_NV = */ 32,
      /* .maxMeshWorkGroupSizeY_NV = */ 1,
      /* .maxMeshWorkGroupSizeZ_NV = */ 1,
      /* .maxTaskWorkGroupSizeX_NV = */ 32,
      /* .maxTaskWorkGroupSizeY_NV = */ 1,
      /* .maxTaskWorkGroupSizeZ_NV = */ 1,
      /* .maxMeshViewCountNV = */ 4,
      /* .maxMeshOutputVerticesEXT = */ 256,
      /* .maxMeshOutputPrimitivesEXT = */ 256,
      /* .maxMeshWorkGroupSizeX_EXT = */ 128,
      /* .maxMeshWorkGroupSizeY_EXT = */ 128,
      /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
      /* .maxTaskWorkGroupSizeX_EXT = */ 128,
      /* .maxTaskWorkGroupSizeY_EXT = */ 128,
      /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
      /* .maxMeshViewCountEXT = */ 4,
      /* .maxDualSourceDrawBuffersEXT = */ 1,

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
  return &default_resource;
}

std::optional<SPIRV::CodeVector>
CompileShaderToSPV(EShLanguage stage, APIType api_type,
                   glslang::EShTargetLanguageVersion language_version, const char* stage_filename,
                   std::string_view source, glslang::TShader::Includer* shader_includer)
{
  if (!InitializeGlslang())
    return std::nullopt;

  std::unique_ptr<glslang::TShader> shader = std::make_unique<glslang::TShader>(stage);
  std::unique_ptr<glslang::TProgram> program;
  glslang::TShader::ForbidIncluder forbid_includer;
  EProfile profile = ECoreProfile;
  EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules);
  if (api_type == APIType::Vulkan || api_type == APIType::Metal)
    messages = static_cast<EShMessages>(messages | EShMsgVulkanRules);
  int default_version = 450;

  const char* pass_source_code = source.data();
  int pass_source_code_length = static_cast<int>(source.size());

  shader->setEnvTarget(glslang::EShTargetSpv, language_version);

  shader->setStringsWithLengths(&pass_source_code, &pass_source_code_length, 1);

  auto DumpBadShader = [&](const char* msg) {
    static int counter = 0;
    std::string filename = VideoBackendBase::BadShaderFilename(stage_filename, counter++);
    std::ofstream stream;
    File::OpenFStream(stream, filename, std::ios_base::out);
    if (stream.good())
    {
      stream << source << std::endl;
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

    stream << "\n";
    stream << "Dolphin Version: " + Common::GetScmRevStr() + "\n";
    stream << "Video Backend: " + g_video_backend->GetDisplayName();
    stream.close();

    PanicAlertFmt("{} (written to {})\nDebug info:\n{}", msg, filename, shader->getInfoLog());
  };

  if (!shader->parse(GetCompilerResourceLimits(), default_version, profile, false, true, messages,
                     shader_includer ? *shader_includer : forbid_includer))
  {
    DumpBadShader("Failed to parse shader");
    return std::nullopt;
  }

  // Even though there's only a single shader, we still need to link it to generate SPV
  program = std::make_unique<glslang::TProgram>();
  program->addShader(shader.get());
  if (!program->link(messages))
  {
    DumpBadShader("Failed to link program");
    return std::nullopt;
  }

  glslang::TIntermediate* intermediate = program->getIntermediate(stage);
  if (!intermediate)
  {
    DumpBadShader("Failed to generate SPIR-V");
    return std::nullopt;
  }

  SPIRV::CodeVector out_code;
  spv::SpvBuildLogger logger;
  glslang::SpvOptions options;

  if (g_ActiveConfig.bEnableValidationLayer)
  {
    // Attach the source code to the SPIR-V for tools like RenderDoc.
    shader->setSourceFile(stage_filename);
    shader->addSourceText(pass_source_code, pass_source_code_length);

    options.generateDebugInfo = true;
    options.disableOptimizer = true;
    options.optimizeSize = false;
    options.disassemble = false;
    options.validate = true;
  }
  else
  {
    options.disableOptimizer = false;
    options.stripDebugInfo = true;
  }

  glslang::GlslangToSpv(*intermediate, out_code, &logger, &options);

  // Write out messages
  // Temporary: skip if it contains "Warning, version 450 is not yet complete; most version-specific
  // features are present, but some are missing."
  if (strlen(shader->getInfoLog()) > 108)
    WARN_LOG_FMT(VIDEO, "Shader info log: {}", shader->getInfoLog());
  if (strlen(shader->getInfoDebugLog()) > 0)
    WARN_LOG_FMT(VIDEO, "Shader debug info log: {}", shader->getInfoDebugLog());
  if (strlen(program->getInfoLog()) > 25)
    WARN_LOG_FMT(VIDEO, "Program info log: {}", program->getInfoLog());
  if (strlen(program->getInfoDebugLog()) > 0)
    WARN_LOG_FMT(VIDEO, "Program debug info log: {}", program->getInfoDebugLog());
  const std::string spv_messages = logger.getAllMessages();
  if (!spv_messages.empty())
    WARN_LOG_FMT(VIDEO, "SPIR-V conversion messages: {}", spv_messages);

  return out_code;
}
}  // namespace

namespace SPIRV
{
std::optional<CodeVector> CompileVertexShader(std::string_view source_code, APIType api_type,
                                              glslang::EShTargetLanguageVersion language_version,
                                              glslang::TShader::Includer* shader_includer)
{
  return CompileShaderToSPV(EShLangVertex, api_type, language_version, "vs", source_code,
                            shader_includer);
}

std::optional<CodeVector> CompileGeometryShader(std::string_view source_code, APIType api_type,
                                                glslang::EShTargetLanguageVersion language_version,
                                                glslang::TShader::Includer* shader_includer)
{
  return CompileShaderToSPV(EShLangGeometry, api_type, language_version, "gs", source_code,
                            shader_includer);
}

std::optional<CodeVector> CompileFragmentShader(std::string_view source_code, APIType api_type,
                                                glslang::EShTargetLanguageVersion language_version,
                                                glslang::TShader::Includer* shader_includer)
{
  return CompileShaderToSPV(EShLangFragment, api_type, language_version, "ps", source_code,
                            shader_includer);
}

std::optional<CodeVector> CompileComputeShader(std::string_view source_code, APIType api_type,
                                               glslang::EShTargetLanguageVersion language_version,
                                               glslang::TShader::Includer* shader_includer)
{
  return CompileShaderToSPV(EShLangCompute, api_type, language_version, "cs", source_code,
                            shader_includer);
}
}  // namespace SPIRV
