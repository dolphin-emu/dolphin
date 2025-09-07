// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ScaleAction.h"

#include <nlohmann/json.hpp>

std::unique_ptr<ScaleAction> ScaleAction::Create(const nlohmann::json& json_data)
{
  Common::Vec3 scale;
  scale.x = json_data.value("X", scale.x);
  scale.y = json_data.value("Y", scale.y);
  scale.z = json_data.value("Z", scale.z);
  return std::make_unique<ScaleAction>(scale);
}

ScaleAction::ScaleAction(Common::Vec3 scale) : m_scale(scale)
{
}

void ScaleAction::OnEFB(GraphicsModActionData::EFB* efb)
{
  if (!efb) [[unlikely]]
    return;

  if (!efb->scaled_width) [[unlikely]]
    return;

  if (!efb->scaled_height) [[unlikely]]
    return;

  if (m_scale.x > 0)
    *efb->scaled_width = efb->texture_width * m_scale.x;

  if (m_scale.y > 0)
    *efb->scaled_height = efb->texture_height * m_scale.y;
}

void ScaleAction::OnProjection(GraphicsModActionData::Projection* projection)
{
  if (!projection) [[unlikely]]
    return;

  if (!projection->matrix) [[unlikely]]
    return;

  auto& matrix = *projection->matrix;
  matrix.data[0] = matrix.data[0] * m_scale.x;
  matrix.data[5] = matrix.data[5] * m_scale.y;
}

void ScaleAction::OnProjectionAndTexture(GraphicsModActionData::Projection* projection)
{
  if (!projection) [[unlikely]]
    return;

  if (!projection->matrix) [[unlikely]]
    return;

  auto& matrix = *projection->matrix;
  matrix.data[0] = matrix.data[0] * m_scale.x;
  matrix.data[5] = matrix.data[5] * m_scale.y;
}
