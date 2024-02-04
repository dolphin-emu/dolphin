// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/TransformAction.h"

#include "Common/JsonUtil.h"

#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

#include <imgui.h>

std::unique_ptr<TransformAction> TransformAction::Create(const picojson::value& json_data)
{
  if (!json_data.is<picojson::object>())
    return nullptr;

  const auto& obj = json_data.get<picojson::object>();

  Common::Vec3 scale{1, 1, 1};
  if (const auto it = obj.find("scale"); it != obj.end())
  {
    if (it->second.is<picojson::object>())
    {
      FromJson(it->second.get<picojson::object>(), scale);
    }
  }

  Common::Vec3 translation{};
  if (const auto it = obj.find("translation"); it != obj.end())
  {
    if (it->second.is<picojson::object>())
    {
      FromJson(it->second.get<picojson::object>(), translation);
    }
  }

  Common::Vec3 rotation{};
  if (const auto it = obj.find("rotation"); it != obj.end())
  {
    if (it->second.is<picojson::object>())
    {
      FromJson(it->second.get<picojson::object>(), rotation);
    }
  }
  return std::make_unique<TransformAction>(std::move(rotation), std::move(scale),
                                           std::move(translation));
}

std::unique_ptr<TransformAction> TransformAction::Create()
{
  return std::make_unique<TransformAction>(Common::Vec3{}, Common::Vec3{1, 1, 1}, Common::Vec3{});
}

TransformAction::TransformAction(Common::Vec3 rotation, Common::Vec3 scale,
                                 Common::Vec3 translation)
    : m_rotation(std::move(rotation)), m_scale(std::move(scale)),
      m_translation(std::move(translation)), m_calculated_transform(Common::Matrix44::Identity()),
      m_transform_changed(true)
{
}

void TransformAction::DrawImGui()
{
  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("TransformTable", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Scale");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Scale", m_scale.data.data()))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
        m_transform_changed = true;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Rotation");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Rotation", m_rotation.data.data()))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
        m_transform_changed = true;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Translate");
      ImGui::TableNextColumn();
      if (ImGui::InputFloat3("##Translate", m_translation.data.data()))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
        m_transform_changed = true;
      }
      ImGui::EndTable();
    }
  }
}

void TransformAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (m_transform_changed)
  {
    const auto scale = Common::Matrix33::Scale(m_scale);
    const auto rotation = Common::Quaternion::RotateXYZ(m_rotation);
    m_calculated_transform = Common::Matrix44::Translate(m_translation) *
                             Common::Matrix44::FromQuaternion(rotation) *
                             Common::Matrix44::FromMatrix33(scale);

    m_transform_changed = false;
  }
  *draw_started->transform = m_calculated_transform;
}

void TransformAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj.emplace("translation", ToJsonObject(m_translation));
  json_obj.emplace("scale", ToJsonObject(m_scale));
  json_obj.emplace("rotation", ToJsonObject(m_rotation));
}

std::string TransformAction::GetFactoryName() const
{
  return "transform";
}
