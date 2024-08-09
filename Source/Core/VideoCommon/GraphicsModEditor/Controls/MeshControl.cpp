// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MeshControl.h"

#include <chrono>

#include <imgui.h>

#include "Common/StringUtil.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

namespace GraphicsModEditor::Controls
{
MeshControl::MeshControl(EditorState& state) : m_state(state)
{
}
void MeshControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                            VideoCommon::MeshData* mesh_data, const std::filesystem::path& path,
                            VideoCommon::CustomAssetLibrary::TimeType* last_data_write,
                            AbstractTexture* mesh_preview)
{
  if (ImGui::BeginTable("MeshForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ID");
    ImGui::TableNextColumn();
    ImGui::Text("%s", asset_id.c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Name");
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", PathToString(path.stem()).c_str());
    ImGui::EndTable();
  }

  if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("MeshMaterialsForm", 2))
    {
      for (auto& [mesh_material, material_asset_id] :
           mesh_data->m_mesh_material_to_material_asset_id)
      {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", mesh_material.c_str());
        ImGui::TableNextColumn();

        if (AssetDisplay(mesh_material, &m_state, &material_asset_id, AssetDataType::Material))
        {
          *last_data_write = std::chrono::system_clock::now();
          GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(material_asset_id);
        }
      }
      ImGui::EndTable();
    }
  }

  if (mesh_preview)
  {
    const auto ratio = mesh_preview->GetWidth() / mesh_preview->GetHeight();
    ImGui::Image(mesh_preview, ImVec2{ImGui::GetContentRegionAvail().x,
                                      ImGui::GetContentRegionAvail().x * ratio});
  }
}
}  // namespace GraphicsModEditor::Controls
