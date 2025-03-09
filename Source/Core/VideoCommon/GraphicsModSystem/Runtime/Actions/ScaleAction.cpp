// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ScaleAction.h"

#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

#include <imgui.h>

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

std::unique_ptr<ScaleAction> ScaleAction::Create()
{
  return std::make_unique<ScaleAction>(Common::Vec3{});
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

void ScaleAction::DrawImGui()
{
  if (ImGui::InputFloat("X", &m_scale.x))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
  }
  if (ImGui::InputFloat("Y", &m_scale.y))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
  }
  if (ImGui::InputFloat("Z", &m_scale.z))
  {
    GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
  }
}

void ScaleAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj["X"] = picojson::value{static_cast<double>(m_scale.x)};
  json_obj["Y"] = picojson::value{static_cast<double>(m_scale.y)};
  json_obj["Z"] = picojson::value{static_cast<double>(m_scale.z)};
}

std::string ScaleAction::GetFactoryName() const
{
  return "scale";
}
