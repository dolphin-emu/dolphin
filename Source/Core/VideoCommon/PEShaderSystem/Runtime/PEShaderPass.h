// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/PEShaderSystem/Runtime/PEBaseShaderPass.h"

#include <memory>

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"

namespace VideoCommon::PE
{
struct ShaderPass final : public BaseShaderPass
{
  std::shared_ptr<AbstractShader> m_vertex_shader;
  std::unique_ptr<AbstractShader> m_pixel_shader;
  std::unique_ptr<AbstractPipeline> m_pipeline;
  std::unique_ptr<AbstractFramebuffer> m_output_framebuffer;
};
}  // namespace VideoCommon::PE
