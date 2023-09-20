// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

class ShaderCode;

namespace VideoCommon
{
// A helper class for a single shader uniform
// The purpose of this class is to ease
// transmitting a block of uniforms to the GPU
class ShaderUniform
{
public:
  ShaderUniform() = default;
  virtual ~ShaderUniform() = default;
  ShaderUniform(const ShaderUniform&) = default;
  ShaderUniform(ShaderUniform&&) = default;
  ShaderUniform& operator=(const ShaderUniform&) = default;
  ShaderUniform& operator=(ShaderUniform&&) = default;
  virtual void WriteToMemory(u8*& buffer) const = 0;
  virtual void WriteAsShaderCode(ShaderCode& shader_source) const = 0;
  virtual std::size_t Size() const = 0;

  virtual std::unique_ptr<ShaderUniform> clone() const = 0;
};
}  // namespace VideoCommon
