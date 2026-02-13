// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomMeshAction.h"

// clang-format off
#include <imgui.h>
#include <ImGuizmo.h>
// clang-format on

#include "Common/JsonUtil.h"
#include "Core/System.h"

#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/EditorMain.h"
#include "VideoCommon/Resources/CustomResourceManager.h"

std::unique_ptr<CustomMeshAction>
CustomMeshAction::Create(const picojson::value& json_data,
                         std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  if (!json_data.is<picojson::object>())
    return nullptr;

  const auto& obj = json_data.get<picojson::object>();

  VideoCommon::CustomAssetLibrary::AssetID mesh_asset;
  bool ignore_mesh_transform = false;

  if (const auto it = obj.find("ignore_mesh_transform"); it != obj.end())
  {
    if (it->second.is<bool>())
    {
      ignore_mesh_transform = it->second.get<bool>();
    }
  }

  if (const auto it = obj.find("mesh_asset"); it != obj.end())
  {
    if (it->second.is<std::string>())
    {
      mesh_asset = it->second.to_str();
    }
  }

  Common::Matrix44 transform = Common::Matrix44::Identity();
  if (const auto it = obj.find("transform"); it != obj.end())
  {
    if (it->second.is<picojson::object>())
    {
      FromJson(it->second.get<picojson::object>(), transform);
    }
  }

  return std::make_unique<CustomMeshAction>(
      std::move(library), SerializableData{.asset_id = std::move(mesh_asset),
                                           .transform = std::move(transform),
                                           .ignore_mesh_transform = ignore_mesh_transform});
}

CustomMeshAction::CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
    : m_library(std::move(library))
{
  m_serializable_data.transform = Common::Matrix44::Identity();
}

CustomMeshAction::CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                                   SerializableData serializable_data)
    : m_library(std::move(library)), m_serializable_data(std::move(serializable_data))
{
}

CustomMeshAction::~CustomMeshAction() = default;

void CustomMeshAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->material) [[unlikely]]
    return;

  if (m_serializable_data.asset_id.empty())
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

  auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();

  const auto mesh = resource_manager.GetMeshFromAsset(m_serializable_data.asset_id,
                                                      *draw_started->draw_data_view.uid, m_library);
  *draw_started->mesh = mesh;
  *draw_started->transform = m_serializable_data.transform;
  *draw_started->ignore_mesh_transform = m_serializable_data.ignore_mesh_transform;
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
                                                    &m_serializable_data.asset_id,
                                                    GraphicsModEditor::Mesh))
      {
        GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(
            m_serializable_data.asset_id);
        m_mesh_asset_changed = true;
      }
      ImGui::EndTable();
    }
  }
  if (ImGui::CollapsingHeader("Custom mesh transform", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("CustomMeshTransform", 2))
    {
      float matrixTranslation[3], matrixRotation[3], matrixScale[3];

      auto transform = m_serializable_data.transform.Transposed();
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

      if (changed)
      {
        ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale,
                                                transform.data.data());
        m_serializable_data.transform = transform.Transposed();
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Ignore Mesh Transform");
      ImGui::TableNextColumn();
      if (ImGui::Checkbox("##IgnoreMeshTransform", &m_serializable_data.ignore_mesh_transform))
      {
        GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip(
            "Ignore any set mesh transform and use only apply the game's transform, this "
            "can be useful when making simple model edits with mesh dumped from Dolphin");
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
  json_obj.emplace("transform", ToJsonObject(m_serializable_data.transform));
  json_obj.emplace("mesh_asset", m_serializable_data.asset_id);
  json_obj.emplace("ignore_mesh_transform", m_serializable_data.ignore_mesh_transform);
}

std::string CustomMeshAction::GetFactoryName() const
{
  return std::string{factory_name};
}

Common::Matrix44* CustomMeshAction::GetTransform()
{
  return &m_serializable_data.transform;
}
