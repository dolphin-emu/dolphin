// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/MeshImportWindow.h"

#include <fstream>
#include <optional>

#include <imgui.h>
#include <picojson.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/GraphicsModEditor/GpuSkinningDataUtils.h"

namespace
{
void ImportMeshData(std::string_view filename, bool process_gpu_skinning)
{
  VideoCommon::MeshData mesh_data;
  if (!VideoCommon::MeshData::FromGLTF(filename, &mesh_data))
  {
    // TODO: move this to the UI
    ERROR_LOG_FMT(VIDEO, "Failed to read GLTF mesh '{}'!", filename);
    return;
  }

  std::string basename;
  std::string basepath;
  SplitPath(filename, &basepath, &basename, nullptr);

  if (process_gpu_skinning)
  {
    const std::string dolphin_reference_mesh_filename = basepath + basename + ".reference.gltf";
    VideoCommon::MeshData reference_mesh_data;
    if (File::Exists(dolphin_reference_mesh_filename))
    {
      if (!VideoCommon::MeshData::FromGLTF(dolphin_reference_mesh_filename, &reference_mesh_data))
      {
        // TODO: move this to the UI
        ERROR_LOG_FMT(VIDEO, "Failed to read GLTF reference mesh '{}'!",
                      dolphin_reference_mesh_filename);
        return;
      }
    }
    else
    {
      // The reference mesh and the original mesh are the same
      // Make a copy
      if (!VideoCommon::MeshData::FromGLTF(filename, &reference_mesh_data))
      {
        // TODO: move this to the UI
        ERROR_LOG_FMT(VIDEO, "Failed to read GLTF mesh '{}'!", filename);
        return;
      }
    }

    const std::string dolphin_json_data_filename = basepath + basename + ".json";
    if (!File::Exists(dolphin_json_data_filename))
    {
      ERROR_LOG_FMT(VIDEO, "Json file '{}' is required when processing gpu skinning!",
                    dolphin_json_data_filename);
      return;
    }
    picojson::value root;
    std::string error;
    if (!JsonFromFile(dolphin_json_data_filename, &root, &error))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to parse json file '{}', error was: '{}'!",
                    dolphin_json_data_filename, error);
      return;
    }
    if (!root.is<picojson::object>())
    {
      ERROR_LOG_FMT(VIDEO, "Json file '{}' has wrong format!", dolphin_json_data_filename);
      return;
    }
    GraphicsModEditor::GpuSkinningData gpu_skinning_data;
    GraphicsModEditor::FromJson(root.get<picojson::object>(), gpu_skinning_data);
    if (gpu_skinning_data.m_draw_call_to_gpu_skinning.empty())
    {
      ERROR_LOG_FMT(VIDEO, "Json file '{}' had no data!", dolphin_json_data_filename);
      return;
    }
    GraphicsModEditor::CalculateMeshDataWithGpuSkinning(gpu_skinning_data, reference_mesh_data,
                                                        mesh_data);
    INFO_LOG_FMT(VIDEO, "Finished parsing");
  }

  const std::string dolphin_mesh_filename = basepath + basename + ".dolmesh";
  File::IOFile outbound_file(dolphin_mesh_filename, "wb");
  if (!VideoCommon::MeshData::ToDolphinMesh(&outbound_file, mesh_data))
  {
    // TODO: move this to the UI
    ERROR_LOG_FMT(VIDEO, "Failed to write Dolphin mesh '{}'", dolphin_mesh_filename);
    return;
  }

  const std::string dolphin_json_filename = basepath + basename + ".metadata";
  picojson::object serialized_root;
  VideoCommon::MeshData::ToJson(serialized_root, mesh_data);
  if (!JsonToFile(dolphin_json_filename, picojson::value{serialized_root}, true))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to write metadata file '{}'", dolphin_json_filename);
  }
}
}  // namespace

namespace GraphicsModEditor::Controls
{
bool ShowMeshImportWindow(std::string_view filename, bool* import_materials,
                          bool* process_gpu_skinning)
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
    ImGui::Checkbox("Process GPU Skinning", process_gpu_skinning);
    ImGui::SetItemTooltip(
        "Process GPU Skinning - given a json file with the original mesh data, the software will "
        "try and build a mapping so that the gpu skinning can be passed to the new mesh at "
        "runtime.  The software matches each position on the new mesh with the old mesh data by "
        "finding the cloest point.  A reference mesh that has the same topology as the new mesh "
        "can be provided to better align the positions.  This process can be time intensive!");

    if (ImGui::Button("Import", ImVec2(120, 0)))
    {
      ImportMeshData(filename, *process_gpu_skinning);
      ImGui::CloseCurrentPopup();
      result = true;
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
