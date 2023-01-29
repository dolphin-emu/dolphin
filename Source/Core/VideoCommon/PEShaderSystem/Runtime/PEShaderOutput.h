// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "VideoCommon/AbstractTexture.h"

namespace VideoCommon::PE
{
struct ShaderOutput
{
  std::unique_ptr<AbstractTexture> m_texture;
  float m_output_scale = 1.0f;
  std::string m_name;
};
}  // namespace VideoCommon::PE
