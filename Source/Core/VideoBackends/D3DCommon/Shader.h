// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string_view>
#include "VideoBackends/D3DCommon/Common.h"
#include "VideoCommon/AbstractShader.h"

namespace D3DCommon
{
class Shader : public AbstractShader
{
public:
  virtual ~Shader() override;

  const BinaryData& GetByteCode() const { return m_bytecode; }

  BinaryData GetBinary() const override;

  static bool CompileShader(D3D_FEATURE_LEVEL feature_level, BinaryData* out_bytecode,
                            ShaderStage stage, std::string_view source);

  static BinaryData CreateByteCode(const void* data, size_t length);

protected:
  Shader(ShaderStage stage, BinaryData bytecode);

  BinaryData m_bytecode;
};

}  // namespace D3DCommon
