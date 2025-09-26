// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/MoveAction.h"

#include <nlohmann/json.hpp>

std::unique_ptr<MoveAction> MoveAction::Create(const nlohmann::json& json_data)
{
  Common::Vec3 position_offset;
  position_offset.x = json_data.value("X", position_offset.x);
  position_offset.y = json_data.value("Y", position_offset.y);
  position_offset.z = json_data.value("Z", position_offset.z);
  return std::make_unique<MoveAction>(position_offset);
}

MoveAction::MoveAction(Common::Vec3 position_offset) : m_position_offset(position_offset)
{
}

void MoveAction::OnProjection(GraphicsModActionData::Projection* projection)
{
  if (!projection) [[unlikely]]
    return;

  if (!projection->matrix) [[unlikely]]
    return;

  auto& matrix = *projection->matrix;
  matrix = matrix * Common::Matrix44::Translate(m_position_offset);
}

void MoveAction::OnProjectionAndTexture(GraphicsModActionData::Projection* projection)
{
  if (!projection) [[unlikely]]
    return;

  if (!projection->matrix) [[unlikely]]
    return;

  auto& matrix = *projection->matrix;
  matrix = matrix * Common::Matrix44::Translate(m_position_offset);
}
