// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

class AbstractFramebuffer;
class AbstractTexture;

namespace VideoCommon::PE
{
struct ShaderApplyOptions
{
  AbstractFramebuffer* m_dest_fb = nullptr;
  MathUtil::Rectangle<int> m_dest_rect = {};
  const AbstractTexture* m_source_color_tex = nullptr;
  const AbstractTexture* m_source_depth_tex = nullptr;
  MathUtil::Rectangle<int> m_source_rect = {};
  int m_source_layer = 0;
  u64 m_time_elapsed = 0;
  float m_camera_tan_half_fov_x = 0.0f;
  float m_camera_tan_half_fov_y = 0.0f;
};
}  // namespace VideoCommon::PE
