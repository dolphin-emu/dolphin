// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MeshExtractWindow.h"

#include <string_view>

#include <imgui.h>

namespace GraphicsModEditor::Controls
{
bool ShowMeshExtractWindow(SceneDumper& scene_dumper, SceneRecordingRequest& request)
{
  bool result = false;

  const std::string_view mesh_extract_popup = "Mesh Extract";
  if (!ImGui::IsPopupOpen(mesh_extract_popup.data()))
  {
    ImGui::OpenPopup(mesh_extract_popup.data());
  }

  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(mesh_extract_popup.data(), nullptr))
  {
    ImGui::Checkbox("Enable Blending", &request.m_enable_blending);
    ImGui::SetItemTooltip("Enable blending - any object marked with blending enabled will have "
                          "transparency turned on for each recorded object in the mesh output.");

    ImGui::Checkbox("Include Materials", &request.m_include_materials);
    ImGui::SetItemTooltip(
        "Include Materials - writes textures to Load/Textures and writes material entries that use "
        "those textures for each recorded object in the mesh output.");

    ImGui::Checkbox("Include Transforms", &request.m_include_transform);
    ImGui::SetItemTooltip("Include Transforms - writes the position, rotation, and scale of "
                          "each recorded object in the mesh output.");

    ImGui::Checkbox("Apply GPU Skinning", &request.m_apply_gpu_skinning);
    ImGui::SetItemTooltip("Apply GPU Skinning - if a mesh uses GPU skinning "
                          " and this is disabled, mesh captured will contain whatever state "
                          "defined by the game (some games may use a T pose or Rest pose), "
                          "otherwise applies the transformation as visible when captured");

    ImGui::Checkbox("Ignore Orthographic Draws", &request.m_ignore_orthographic);
    ImGui::SetItemTooltip(
        "Ignore Orthographic Draws - ignores draws done using an orthographic projection. "
        "This typically includes 2d elements like the HUD or EFB copies.");

    ImGui::Checkbox("Dump GPU Skinning as Json", &request.m_dump_gpu_skinning_as_json);
    ImGui::SetItemTooltip("Dump GPU Skinning as Json - if a mesh uses GPU skinning "
                          " then the positions and transform data will be dumped to a json file. "
                          "This can be used for skinning replacement");

    if (ImGui::Button("Extract", ImVec2(120, 0)))
    {
      scene_dumper.Record(request);
      ImGui::CloseCurrentPopup();
      result = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0)))
    {
      ImGui::CloseCurrentPopup();
      result = true;
    }
    ImGui::EndPopup();
  }
  return result;
}
}  // namespace GraphicsModEditor::Controls
