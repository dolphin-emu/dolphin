// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderInput.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderOutput.h"

class ShaderCode;

namespace VideoCommon::PE
{
struct BaseShaderPass
{
  BaseShaderPass() = default;
  virtual ~BaseShaderPass() = default;

  BaseShaderPass(const BaseShaderPass&) noexcept = delete;
  BaseShaderPass(BaseShaderPass&&) noexcept = default;
  BaseShaderPass& operator=(const BaseShaderPass&) noexcept = delete;
  BaseShaderPass& operator=(BaseShaderPass&&) noexcept = default;

  std::vector<std::unique_ptr<ShaderInput>> m_inputs;

  std::unique_ptr<AbstractTexture> m_output_texture;
  float m_output_scale;
  std::vector<ShaderOutput*> m_additional_outputs;

  void WriteShaderIndices(ShaderCode& shader_source,
                          const std::vector<std::string>& named_outputs) const;
};
}  // namespace VideoCommon::PE
