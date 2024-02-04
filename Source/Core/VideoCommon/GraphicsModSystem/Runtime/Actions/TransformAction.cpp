// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/TransformAction.h"

#define USE_IMGUI_API

// clang-format off
#include <imgui.h>
#include <ImGuizmo.h>
// clang-format on

#include "Common/JsonUtil.h"

#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

std::unique_ptr<TransformAction> TransformAction::Create(const picojson::value& json_data)
{
  if (!json_data.is<picojson::object>())
    return nullptr;

  const auto& obj = json_data.get<picojson::object>();

  Common::Matrix44 transform;
  if (const auto it = obj.find("transform"); it != obj.end())
  {
    if (it->second.is<picojson::object>())
    {
      FromJson(it->second.get<picojson::object>(), transform);
    }
  }

  return std::make_unique<TransformAction>(std::move(transform));
}

std::unique_ptr<TransformAction> TransformAction::Create()
{
  return std::make_unique<TransformAction>(Common::Matrix44::Identity());
}

TransformAction::TransformAction(Common::Matrix44 transform) : m_transform(transform)
{
}

void TransformAction::DrawImGui()
{
  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("TransformTable", 2))
    {
      float matrixTranslation[3], matrixRotation[3], matrixScale[3];

      auto transform = m_transform.Transposed();
      ImGuizmo::DecomposeMatrixToComponents(transform.data.data(), matrixTranslation,
                                            matrixRotation, matrixScale);

      bool changed = false;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Scale");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Scale", matrixScale))
      {
        GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
        changed = true;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Rotation");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Rotation", matrixRotation))
      {
        GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
        changed = true;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Translate");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Translate", matrixTranslation))
      {
        GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
        changed = true;
      }
      ImGui::EndTable();

      if (changed)
      {
        ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale,
                                                transform.data.data());
        m_transform = transform.Transposed();
      }
    }
  }
}

void TransformAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  *draw_started->transform = m_transform;
}

void TransformAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj.emplace("transform", ToJsonObject(m_transform));
}

std::string TransformAction::GetFactoryName() const
{
  return "transform";
}

Common::Matrix44* TransformAction::GetTransform()
{
  return &m_transform;
}
