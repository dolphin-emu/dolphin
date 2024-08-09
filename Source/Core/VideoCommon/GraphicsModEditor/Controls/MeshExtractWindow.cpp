// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MeshExtractWindow.h"

#include <string_view>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <imgui.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

namespace GraphicsModEditor::Controls
{
bool ShowMeshExtractWindow(SceneDumper& scene_dumper, SceneDumper::RecordingRequest& request)
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

    if (ImGui::Button("Extract", ImVec2(120, 0)))
    {
      const std::string path_prefix =
          File::GetUserPath(D_DUMPMESHES_IDX) + SConfig::GetInstance().GetGameID();

      const std::time_t start_time = std::time(nullptr);
      const std::string base_name =
          fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}", path_prefix, fmt::localtime(start_time));
      const std::string path = fmt::format("{}.gltf", base_name);
      scene_dumper.Record(path, request);
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
