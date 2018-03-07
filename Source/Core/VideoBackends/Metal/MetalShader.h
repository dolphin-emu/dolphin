// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/AbstractShader.h"

namespace Metal
{
class MetalShader final : public AbstractShader
{
public:
  MetalShader(ShaderStage stage, const std::string& msl_source, mtlpp::Library library,
              mtlpp::Function function);
  ~MetalShader() override;

  mtlpp::Library GetLibrary() const;
  mtlpp::Function GetFunction() const;

  bool HasBinary() const override;
  BinaryData GetBinary() const override;

  static std::unique_ptr<MetalShader> CreateFromSource(ShaderStage stage,
                                                       const std::string& source);
  static std::unique_ptr<MetalShader> CreateFromBinary(ShaderStage stage, const void* data,
                                                       size_t length);

private:
  std::string m_msl_source;
  mtlpp::Library m_library;
  mtlpp::Function m_function;
};

}  // namespace Metal
