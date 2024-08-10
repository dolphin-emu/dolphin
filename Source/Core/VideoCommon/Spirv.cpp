// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Spirv.h"

// glslang includes
#include "GlslangToSpv.h"
#include "ResourceLimits.h"
#include "disassemble.h"

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

  std::atexit([]() { glslang::FinalizeProcess(); });

  glslang_initialized = true;
  return true;
}

const TBuiltInResource* GetCompilerResourceLimits()
{
  return &glslang::DefaultTBuiltInResource;
}

std::optional<SPIRV::CodeVector>
CompileShaderToSPV(EShLanguage stage, const APIType api_type,
                   const glslang::EShTargetLanguageVersion language_version, const char* stage_filename,
                   const std::string_view source)
{
  if (!InitializeGlslang())
    return std::nullopt;

  const auto shader = std::make_unique<glslang::TShader>(stage);
  std::unique_ptr<glslang::TProgram> program;
  glslang::TShader::ForbidIncluder includer;
  constexpr EProfile profile = ECoreProfile;
  auto messages = static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules);
  if (api_type == APIType::Vulkan || api_type == APIType::Metal)
    messages = static_cast<EShMessages>(messages | EShMsgVulkanRules);
  constexpr int default_version = 450;

  const char* pass_source_code = source.data();
  const int pass_source_code_length = static_cast<int>(source.size());

  shader->setEnvTarget(glslang::EShTargetSpv, language_version);

  shader->setStringsWithLengths(&pass_source_code, &pass_source_code_length, 1);

  auto DumpBadShader = [&](const char* msg) {
    static int counter = 0;
    const std::string filename = VideoBackendBase::BadShaderFilename(stage_filename, counter++);
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
                     includer))
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
    intermediate->setSourceFile(stage_filename);
    intermediate->addSourceText(pass_source_code, pass_source_code_length);

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

  GlslangToSpv(*intermediate, out_code, &logger, &options);

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
std::optional<CodeVector> CompileVertexShader(const std::string_view source_code, const APIType api_type,
                                              const glslang::EShTargetLanguageVersion language_version)
{
  return CompileShaderToSPV(EShLangVertex, api_type, language_version, "vs", source_code);
}

std::optional<CodeVector> CompileGeometryShader(const std::string_view source_code, const APIType api_type,
                                                const glslang::EShTargetLanguageVersion language_version)
{
  return CompileShaderToSPV(EShLangGeometry, api_type, language_version, "gs", source_code);
}

std::optional<CodeVector> CompileFragmentShader(const std::string_view source_code, const APIType api_type,
                                                const glslang::EShTargetLanguageVersion language_version)
{
  return CompileShaderToSPV(EShLangFragment, api_type, language_version, "ps", source_code);
}

std::optional<CodeVector> CompileComputeShader(const std::string_view source_code, const APIType api_type,
                                               const glslang::EShTargetLanguageVersion language_version)
{
  return CompileShaderToSPV(EShLangCompute, api_type, language_version, "cs", source_code);
}
}  // namespace SPIRV
