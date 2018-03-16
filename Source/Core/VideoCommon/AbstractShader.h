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
  using BinaryData = std::vector<u8>;
  virtual bool HasBinary() const = 0;
  virtual BinaryData GetBinary() const = 0;

protected:
  ShaderStage m_stage;
};
