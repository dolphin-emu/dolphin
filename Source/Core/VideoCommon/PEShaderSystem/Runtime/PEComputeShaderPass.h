// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEBaseShaderPass.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderInput.h"

namespace VideoCommon::PE
{
struct ComputeShaderPass final : public BaseShaderPass
{
  std::unique_ptr<AbstractShader> m_compute_shader;
  std::unique_ptr<AbstractTexture> m_compute_texture;
  std::vector<std::unique_ptr<AbstractTexture>> m_additional_compute_textures;
};
}  // namespace VideoCommon::PE
