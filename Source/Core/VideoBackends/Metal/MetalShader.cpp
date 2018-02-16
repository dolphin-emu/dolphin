// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <sstream>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/MetalShader.h"
#include "VideoBackends/Metal/ShaderCompiler.h"

namespace Metal
{
static constexpr const char* ENTRY_POINT_FUNCTION = "main0";

MetalShader::MetalShader(ShaderStage stage, const std::string& msl_source, mtlpp::Library library,
                         mtlpp::Function function)
    : AbstractShader(stage), m_msl_source(msl_source), m_library(library), m_function(function)
{
}

MetalShader::~MetalShader()
{
}

mtlpp::Library MetalShader::GetLibrary() const
{
  return m_library;
}

mtlpp::Function MetalShader::GetFunction() const
{
  return m_function;
}

bool MetalShader::HasBinary() const
{
  return !m_msl_source.empty();
}

AbstractShader::BinaryData MetalShader::GetBinary() const
{
  BinaryData data(m_msl_source.size());
  std::memcpy(data.data(), m_msl_source.c_str(), m_msl_source.length());
  return data;
}

static void LogCompileError(ShaderStage stage, const char* part, const std::string& source,
                            const ns::Error& err)
{
  std::stringstream ss;
  ss << "Failed to compile Metal " << ShaderCompiler::GetStagePrefix(stage) << " to " << part
     << ".\n";
  ss << "Error Code: " << err.GetCode() << " Domain: " << err.GetDomain().GetCStr() << "\n";
  ss << "Error Message: " << err.GetLocalizedDescription().GetCStr() << "\n";
  ss << "Shader Source:\n" << source << "\n";
  ERROR_LOG(VIDEO, "%s", ss.str().c_str());

  static int counter = 0;
  std::string filename =
      StringFromFormat("%sbad_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(),
                       ShaderCompiler::GetStagePrefix(stage), counter++);
  std::ofstream stream;
  File::OpenFStream(stream, filename, std::ios::out);
  if (stream.good())
    stream << ss.str();
}

static std::unique_ptr<MetalShader> CompileShaderLibrary(ShaderStage stage,
                                                         const std::string& msl_source)
{
  ns::Error error;
  mtlpp::CompileOptions options;
  mtlpp::Library library =
      g_metal_context->GetDevice().NewLibrary(msl_source.c_str(), options, &error);
  if (!library)
  {
    LogCompileError(stage, "library", msl_source, error);
    return nullptr;
  }

  mtlpp::FunctionConstantValues constants;
  mtlpp::Function func = library.NewFunction(ENTRY_POINT_FUNCTION, constants, &error);
  if (!func)
  {
    LogCompileError(stage, "function", msl_source, error);
    return nullptr;
  }

  return std::make_unique<MetalShader>(stage, msl_source, library, func);
}

std::unique_ptr<MetalShader> MetalShader::CreateFromSource(ShaderStage stage,
                                                           const std::string& source)
{
  ShaderCompiler::SPIRVCodeVector spv;
  if (!ShaderCompiler::CompileShaderToSPV(&spv, stage, source.c_str(), source.length()))
    return nullptr;

  std::string msl_source;
  if (!ShaderCompiler::TranslateSPVToMSL(&msl_source, stage, &spv))
    return nullptr;

  return CompileShaderLibrary(stage, msl_source);
}

std::unique_ptr<MetalShader> MetalShader::CreateFromBinary(ShaderStage stage, const void* data,
                                                           size_t length)
{
  // This assumes that the "binary" contains MSL source.
  if (length == 0)
    return nullptr;

  std::string msl_source;
  msl_source.resize(length);
  std::memcpy(&msl_source[0], data, length);
  return CompileShaderLibrary(stage, msl_source);
}

}  // namespace Metal
