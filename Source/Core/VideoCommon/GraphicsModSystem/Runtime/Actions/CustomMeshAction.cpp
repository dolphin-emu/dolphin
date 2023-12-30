// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomMeshAction.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/JsonUtil.h"
#include "Core/System.h"

#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/EditorMain.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomResourceManager.h"

std::unique_ptr<CustomMeshAction>
CustomMeshAction::Create(const picojson::value& json_data,
                         std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  VideoCommon::CustomAssetLibrary::AssetID mesh_asset;

  if (!json_data.is<picojson::object>())
    return nullptr;

  const auto& obj = json_data.get<picojson::object>();

  if (const auto it = obj.find("mesh_asset"); it != obj.end())
  {
    if (it->second.is<std::string>())
    {
      mesh_asset = it->second.to_str();
    }
  }

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

  return std::make_unique<CustomMeshAction>(std::move(library), std::move(rotation),
                                            std::move(scale), std::move(translation),
                                            std::move(mesh_asset));
}

CustomMeshAction::CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
    : m_library(std::move(library))
{
}

CustomMeshAction::CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                                   Common::Vec3 rotation, Common::Vec3 scale,
                                   Common::Vec3 translation,
                                   VideoCommon::CustomAssetLibrary::AssetID mesh_asset_id)
    : m_library(std::move(library)), m_mesh_asset_id(std::move(mesh_asset_id)),
      m_scale(std::move(scale)), m_rotation(std::move(rotation)),
      m_translation(std::move(translation))
{
  m_transform_changed = true;
}

CustomMeshAction::~CustomMeshAction() = default;

void CustomMeshAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->material) [[unlikely]]
    return;

  if (m_mesh_asset_id.empty())
    return;

  /*if (m_recalculate_original_mesh_center)
  {
    auto vert_ptr = draw_started->draw_data_view.vertex_data.data();
    Common::Vec3 center_point{};
    for (std::size_t vert_index = 0; vert_index < draw_started->draw_data_view.vertex_data.size();
         vert_index++)
    {
      float vx = 0;
      std::memcpy(&vx, vert_ptr, sizeof(float));
      center_point.x += vx;

      float vy = 0;
      std::memcpy(&vy, vert_ptr + sizeof(float), sizeof(float));
      center_point.y += vy;

      float vz = 0;
      std::memcpy(&vz, vert_ptr + sizeof(float) * 2, sizeof(float));
      center_point.z += vz;

      vert_ptr += draw_started->draw_data_view.vertex_format->GetVertexStride();
    }
    center_point.x /= draw_started->draw_data_view.vertex_data.size();
    center_point.y /= draw_started->draw_data_view.vertex_data.size();
    center_point.z /= draw_started->draw_data_view.vertex_data.size();
    m_original_mesh_center = center_point;
  }*/

  if (m_transform_changed)
  {
    const auto scale = Common::Matrix33::Scale(m_scale);
    const auto rotation = Common::Quaternion::RotateXYZ(m_rotation);
    m_calculated_transform = Common::Matrix44::FromMatrix33(scale) *
                             Common::Matrix44::FromQuaternion(rotation) *
                             Common::Matrix44::Translate(m_translation);
    m_transform_changed = false;
  }

  auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();

  const auto mesh =
      resource_manager.GetMeshFromAsset(m_mesh_asset_id, m_library, draw_started->draw_data_view);
  *draw_started->mesh = mesh;

  if (mesh)
  {
    *draw_started->transform = m_calculated_transform;
    *draw_started->ignore_mesh_transform = m_ignore_mesh_transform;
  }
}

void CustomMeshAction::DrawImGui()
{
  auto& editor = Core::System::GetInstance().GetGraphicsModEditor();
  if (ImGui::CollapsingHeader("Custom mesh", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("CustomMeshForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Mesh");
      ImGui::TableNextColumn();
      if (GraphicsModEditor::Controls::AssetDisplay("MeshValue", editor.GetEditorState(),
                                                    &m_mesh_asset_id, GraphicsModEditor::Mesh))
      {
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(m_mesh_asset_id);
        m_mesh_asset_changed = true;
      }
      ImGui::EndTable();
    }
  }
  if (ImGui::CollapsingHeader("Custom mesh transform", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("CustomMeshTransform", 2))
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
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Ignore Mesh Transform");
      ImGui::TableNextColumn();
      ImGui::SetTooltip(
          "Ignore any set mesh transform and use only apply the game's transform, this "
          "can be useful when making simple model edits with mesh dumped from Dolphin");
      if (ImGui::Checkbox("##IgnoreMeshTransform", &m_ignore_mesh_transform))
      {
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();
        m_transform_changed = true;
      }
      ImGui::EndTable();
    }
  }
}

void CustomMeshAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj.emplace("translation", ToJsonObject(m_translation));
  json_obj.emplace("scale", ToJsonObject(m_scale));
  json_obj.emplace("rotation", ToJsonObject(m_rotation));
  json_obj.emplace("mesh_asset", m_mesh_asset_id);
  json_obj.emplace("ignore_mesh_transform", m_ignore_mesh_transform);
}

std::string CustomMeshAction::GetFactoryName() const
{
  return std::string{factory_name};
}
