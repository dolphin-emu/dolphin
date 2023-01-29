// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigOption.h"

class ShaderCode;

namespace VideoCommon::PE
{
class ShaderOption
{
public:
  static std::unique_ptr<ShaderOption> Create(const ShaderConfigOption& config_option);
  ShaderOption() = default;
  virtual ~ShaderOption() = default;
  ShaderOption(const ShaderOption&) = default;
  ShaderOption(ShaderOption&&) = default;
  ShaderOption& operator=(const ShaderOption&) = default;
  ShaderOption& operator=(ShaderOption&&) = default;
  virtual void Update(const ShaderConfigOption& config_option) = 0;
  void WriteToMemory(u8*& buffer) const;
  void WriteShaderUniforms(ShaderCode& shader_source) const;
  void WriteShaderConstants(ShaderCode& shader_source) const;
  std::size_t Size() const;

protected:
  bool m_evaluate_at_compile_time = false;

private:
  virtual std::size_t SizeImpl() const = 0;
  virtual void WriteToMemoryImpl(u8*& buffer) const = 0;
  virtual void WriteShaderUniformsImpl(ShaderCode& shader_source) const = 0;
  virtual void WriteShaderConstantsImpl(ShaderCode& shader_source) const = 0;
};
}  // namespace VideoCommon::PE
