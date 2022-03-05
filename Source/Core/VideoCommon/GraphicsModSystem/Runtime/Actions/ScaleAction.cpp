// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ScaleAction.h"

std::unique_ptr<ScaleAction> ScaleAction::Create(const picojson::value& json_data)
{
  Common::Vec3 scale;
  const auto& x = json_data.get("X");
  if (x.is<double>())
  {
    scale.x = static_cast<float>(x.get<double>());
  }

  const auto& y = json_data.get("Y");
  if (y.is<double>())
  {
    scale.y = static_cast<float>(y.get<double>());
  }

  const auto& z = json_data.get("Z");
  if (z.is<double>())
  {
    scale.z = static_cast<float>(z.get<double>());
  }
  return std::make_unique<ScaleAction>(scale);
}

ScaleAction::ScaleAction(Common::Vec3 scale) : m_scale(scale)
{
}

void ScaleAction::OnEFB(bool*, u32 texture_width, u32 texture_height, u32* scaled_width,
                        u32* scaled_height)
{
  if (scaled_width && m_scale.x > 0)
    *scaled_width = texture_width * m_scale.x;

  if (scaled_height && m_scale.y > 0)
    *scaled_height = texture_height * m_scale.y;
}

void ScaleAction::OnProjection(Common::Matrix44* matrix)
{
  if (!matrix)
    return;
  auto& the_matrix = *matrix;
  the_matrix.data[0] = the_matrix.data[0] * m_scale.x;
  the_matrix.data[5] = the_matrix.data[5] * m_scale.y;
}

void ScaleAction::OnProjectionAndTexture(Common::Matrix44* matrix)
{
  if (!matrix)
    return;
  auto& the_matrix = *matrix;
  the_matrix.data[0] = the_matrix.data[0] * m_scale.x;
  the_matrix.data[5] = the_matrix.data[5] * m_scale.y;
}
