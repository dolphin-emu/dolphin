// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

enum class ShaderStage
{
  Vertex,
  Geometry,
  Pixel,
  Compute
};

class AbstractShader
{
public:
  explicit AbstractShader(ShaderStage stage) : m_stage(stage) {}
  virtual ~AbstractShader() = default;

  ShaderStage GetStage() const { return m_stage; }

  // Shader binaries represent the input source code in a lower-level form. e.g. SPIR-V or DXBC.
  // The shader source code is not required to create a shader object from the binary.
  using BinaryData = std::vector<u8>;
  virtual BinaryData GetBinary() const { return {}; }

protected:
  ShaderStage m_stage;
};
