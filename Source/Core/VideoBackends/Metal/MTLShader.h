// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.h>

#include "VideoBackends/Metal/MRCHelpers.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"

namespace Metal
{
class Shader : public AbstractShader
{
public:
  explicit Shader(ShaderStage stage, std::string msl, MRCOwned<id<MTLFunction>> shader);
  ~Shader();

  id<MTLFunction> GetShader() const { return m_shader; }
  BinaryData GetBinary() const override;

private:
  std::string m_msl;
  MRCOwned<id<MTLFunction>> m_shader;
};
}  // namespace Metal
