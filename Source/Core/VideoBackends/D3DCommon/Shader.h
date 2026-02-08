// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string_view>
#include "VideoBackends/D3DCommon/D3DCommon.h"
#include "VideoCommon/AbstractShader.h"

namespace D3DCommon
{
class Shader : public AbstractShader
{
public:
  virtual ~Shader() override;

  const BinaryData& GetByteCode() const { return m_bytecode; }

  BinaryData GetBinary() const override;

  static std::optional<BinaryData> CompileShader(D3D_FEATURE_LEVEL feature_level, ShaderStage stage,
                                                 std::string_view source);

  static BinaryData CreateByteCode(const void* data, size_t length);

protected:
  Shader(ShaderStage stage, BinaryData bytecode);

  BinaryData m_bytecode;
};

}  // namespace D3DCommon
