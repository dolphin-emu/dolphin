// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "VideoCommon/PEShaderSystem/Runtime/PEShaderApplyOptions.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderOption.h"
#include "VideoCommon/PEShaderSystem/Runtime/RuntimeShaderImpl.h"

class AbstractTexture;
class ShaderCode;

namespace VideoCommon::PE
{
class BuiltinUniforms
{
public:
  BuiltinUniforms();
  std::size_t Size() const;
  void WriteToMemory(u8*& buffer) const;
  void WriteShaderUniforms(ShaderCode& shader_source) const;
  void Update(const ShaderApplyOptions& options, const AbstractTexture* last_pass_texture,
              float last_pass_output_scale);

private:
  std::array<const ShaderOption*, 11> GetOptions() const;
  RuntimeFloatOption<4> m_prev_resolution;
  RuntimeFloatOption<4> m_prev_rect;
  RuntimeFloatOption<4> m_src_resolution;
  RuntimeFloatOption<4> m_window_resolution;
  RuntimeFloatOption<4> m_src_rect;
  RuntimeIntOption<1> m_time;
  RuntimeIntOption<1> m_layer;
  RuntimeFloatOption<1> m_output_scale;
  RuntimeFloatOption<2> m_ndc_to_view_mul;
  RuntimeFloatOption<2> m_ndc_to_view_add;
  RuntimeIntOption<1> m_frames;
};
}  // namespace VideoCommon::PE
