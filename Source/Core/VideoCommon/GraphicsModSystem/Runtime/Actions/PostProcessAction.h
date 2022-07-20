// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "Common/Timer.h"

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigGroup.h"

namespace VideoCommon::PE
{
class ShaderGroup;
}

class PostProcessAction final : public GraphicsModAction
{
public:
  static std::unique_ptr<PostProcessAction> Create(const picojson::value& json_data,
                                                   std::string_view path);
  explicit PostProcessAction(VideoCommon::PE::ShaderConfigGroup config);
  ~PostProcessAction();

  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;
  void OnEFB(GraphicsModActionData::EFB*) override;
  void OnProjection(GraphicsModActionData::Projection*) override;
  void OnProjectionAndTexture(GraphicsModActionData::Projection*) override;

  void OnFrameEnd() override;

private:
  VideoCommon::PE::ShaderConfigGroup m_config;
  std::unique_ptr<VideoCommon::PE::ShaderGroup> m_pp_group;
  std::unique_ptr<VideoCommon::PE::ShaderGroup> m_default_shader_group;

  float m_camera_tan_half_fov_x = 0.0f;
  float m_camera_tan_half_fov_y = 0.0f;

  Common::Timer m_timer;

  bool m_applied = false;
};
