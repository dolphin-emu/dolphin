// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/MoveAction.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

#include <imgui.h>

std::unique_ptr<MoveAction> MoveAction::Create(const picojson::value& json_data)
{
  Common::Vec3 position_offset;
  const auto& x = json_data.get("X");
  if (x.is<double>())
  {
    position_offset.x = static_cast<float>(x.get<double>());
  }

  const auto& y = json_data.get("Y");
  if (y.is<double>())
  {
    position_offset.y = static_cast<float>(y.get<double>());
  }

  const auto& z = json_data.get("Z");
  if (z.is<double>())
  {
    position_offset.z = static_cast<float>(z.get<double>());
  }
  return std::make_unique<MoveAction>(position_offset);
}

std::unique_ptr<MoveAction> MoveAction::Create()
{
  return std::make_unique<MoveAction>(Common::Vec3{});
}

MoveAction::MoveAction(Common::Vec3 position_offset) : m_position_offset(position_offset)
{
}

void MoveAction::DrawImGui()
{
  if (ImGui::InputFloat("X", &m_position_offset.x))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
  }
  if (ImGui::InputFloat("Y", &m_position_offset.y))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
  }
  if (ImGui::InputFloat("Z", &m_position_offset.z))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
  }
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

void MoveAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj["X"] = picojson::value{static_cast<double>(m_position_offset.x)};
  json_obj["Y"] = picojson::value{static_cast<double>(m_position_offset.y)};
  json_obj["Z"] = picojson::value{static_cast<double>(m_position_offset.z)};
}

std::string MoveAction::GetFactoryName() const
{
  return "move";
}
