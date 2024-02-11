// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MeshImportWindow.h"

#include <fstream>

#include <imgui.h>
#include <picojson.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoCommon/Assets/MeshAsset.h"

namespace GraphicsModEditor::Controls
{
bool ShowMeshImportWindow(std::string_view filename, bool* import_materials)
{
  bool result = false;
  const std::string_view mesh_import_popup = "Mesh Import";
  if (!ImGui::IsPopupOpen(mesh_import_popup.data()))
  {
    ImGui::OpenPopup(mesh_import_popup.data());
  }

  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(mesh_import_popup.data(), nullptr))
  {
    ImGui::BeginDisabled();
    ImGui::Checkbox("Import Materials", import_materials);
    ImGui::SetItemTooltip(
        "Import Materials - materials from the mesh will be created as Dolphin materials.");
    ImGui::EndDisabled();

    if (ImGui::Button("Import", ImVec2(120, 0)))
    {
      VideoCommon::MeshData mesh_data;
      if (!VideoCommon::MeshData::FromGLTF(filename, &mesh_data))
      {
        // TODO: move this to the UI
        ERROR_LOG_FMT(VIDEO, "Failed to read GLTF mesh '{}'", filename);
      }
      else
      {
        std::string basename;
        std::string basepath;
        SplitPath(filename, &basepath, &basename, nullptr);

        const std::string dolphin_mesh_filename = basepath + basename + ".dolmesh";
        File::IOFile outbound_file(dolphin_mesh_filename, "wb");
        if (!VideoCommon::MeshData::ToDolphinMesh(&outbound_file, mesh_data))
        {
          // TODO: move this to the UI
          ERROR_LOG_FMT(VIDEO, "Failed to write Dolphin mesh '{}'", dolphin_mesh_filename);
        }
        else
        {
          const std::string dolphin_json_filename = basepath + basename + ".metadata";
          std::ofstream json_stream;
          File::OpenFStream(json_stream, dolphin_json_filename, std::ios_base::out);
          if (!json_stream.is_open())
          {
            ERROR_LOG_FMT(VIDEO, "Failed to open metadata file '{}' for writing",
                          dolphin_json_filename);
            ImGui::CloseCurrentPopup();
          }
          else
          {
            picojson::object serialized_root;
            VideoCommon::MeshData::ToJson(serialized_root, mesh_data);
            const auto output = picojson::value{serialized_root}.serialize(true);
            json_stream << output;
          }
        }

        ImGui::CloseCurrentPopup();
        result = true;
      }
    }

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
