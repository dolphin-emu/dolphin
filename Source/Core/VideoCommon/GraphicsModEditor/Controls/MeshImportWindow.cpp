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
void ImportMeshData(std::string_view filename, u32 components_to_copy, bool parse_cpu_skinning)
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

  if (components_to_copy > 0 || parse_cpu_skinning)
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

    // We should do more verification than this but at least check the sizes to make sure
    // they match
    if (reference_mesh_data.m_mesh_chunks.size() != mesh_data.m_mesh_chunks.size())
    {
      ERROR_LOG_FMT(VIDEO, "Reference mesh and replacement mesh do not have the same size!");
      return;
    }

    const std::string dolphin_data_filename = basepath + basename + ".nativemesh";
    File::IOFile file(dolphin_data_filename, "rb");
    if (!file.IsOpen())
    {
      ERROR_LOG_FMT(VIDEO, "Native mesh file '{}' could not be opened!", dolphin_data_filename);
      return;
    }

    GraphicsModEditor::NativeMeshData native_mesh_data;
    if (!GraphicsModEditor::NativeMeshFromFile(&file, &native_mesh_data))
    {
      ERROR_LOG_FMT(VIDEO, "Native mesh file '{}' could not be parsed!", dolphin_data_filename);
      return;
    }

    std::optional<GraphicsModEditor::ExporterSkinningRig> exported_skinning_rig;
    if (parse_cpu_skinning)
    {
      const std::string dolphin_rig_filename = basepath + basename + ".rig";
      File::IOFile rig_file(dolphin_rig_filename, "rb");
      if (!rig_file.IsOpen())
      {
        ERROR_LOG_FMT(VIDEO, "Rig file '{}' could not be opened!", dolphin_rig_filename);
        return;
      }

      exported_skinning_rig.emplace();

      std::vector<u8> rig_bytes;
      rig_bytes.reserve(rig_file.GetSize());
      rig_file.ReadBytes(rig_bytes.data(), rig_file.GetSize());
      if (!GraphicsModEditor::ExporterSkinningRig::FromBinary(rig_bytes, &*exported_skinning_rig))
      {
        ERROR_LOG_FMT(VIDEO, "Error processing rig file '{}'!", dolphin_rig_filename);
        return;
      }
    }

    ERROR_LOG_FMT(VIDEO, "Mesh data before computing skinning");
    mesh_data.Report();

    GraphicsModEditor::CalculateMeshDataWithSkinning(native_mesh_data, reference_mesh_data,
                                                     components_to_copy, exported_skinning_rig,
                                                     &mesh_data);
    ERROR_LOG_FMT(VIDEO, "Mesh data after skinning");
    mesh_data.Report();
  }

  const std::string dolphin_mesh_filename = basepath + basename + ".dolmesh";
  {
    File::IOFile outbound_file(dolphin_mesh_filename, "wb");
    if (!VideoCommon::MeshData::ToDolphinMesh(&outbound_file, mesh_data))
    {
      // TODO: move this to the UI
      ERROR_LOG_FMT(VIDEO, "Failed to write Dolphin mesh '{}'", dolphin_mesh_filename);
      return;
    }
  }

  {
    File::IOFile file(dolphin_mesh_filename, "rb");
    if (!file.IsOpen())
    {
      return;
    }

    std::vector<u8> bytes;
    bytes.reserve(file.GetSize());
    file.ReadBytes(bytes.data(), file.GetSize());
    VideoCommon::MeshData mesh_data_read;
    if (!VideoCommon::MeshData::FromDolphinMesh(bytes, &mesh_data_read))
    {
      return;
    }
    ERROR_LOG_FMT(VIDEO, "Mesh data written to disk");
    mesh_data_read.Report();
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
                          bool* treat_as_cpu_skinned, u32* components_to_copy)
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
    if (ImGui::IsItemHovered())
    {
      ImGui::SetItemTooltip(
          "Import Materials - materials from the mesh will be created as Dolphin materials.");
    }
    ImGui::EndDisabled();
    ImGui::BeginGroup();
    ImGui::CheckboxFlags("GPU Skinning Index", components_to_copy, VB_HAS_POSMTXIDX);
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip(
          "Copies the GPU skinning index to the new mesh if it exists on the original "
          "mesh.  Dolphin will try to apply the game's animation to the new mesh "
          "at runtime.");
    }
    ImGui::CheckboxFlags("Color Channel 0", components_to_copy, VB_HAS_COL0);
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip(
          "Copies the first color channel to the new mesh if it exists on the original "
          "mesh.  Overwrites any existing data.");
    }
    ImGui::CheckboxFlags("Color Channel 1", components_to_copy, VB_HAS_COL1);
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip(
          "Copies the second color channel to the new mesh if it exists on the original "
          "mesh.  Overwrites any existing data");
    }
    ImGui::CheckboxFlags("Texcoord Channel 0", components_to_copy, VB_HAS_UV0);
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip(
          "Copies the first texcoord channel to the new mesh if it exists on the original "
          "mesh.  Overwrites any existing data.");
    }
    ImGui::CheckboxFlags("Texcoord Channel 1", components_to_copy, VB_HAS_UV1);
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip(
          "Copies the second texcoord channel to the new mesh if it exists on the original "
          "mesh.  Overwrites any existing data");
    }
    ImGui::CheckboxFlags("Texcoord Channel 2", components_to_copy, VB_HAS_UV2);
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip(
          "Copies the third texcoord channel to the new mesh if it exists on the original "
          "mesh.  Overwrites any existing data");
    }
    ImGui::EndGroup();

    ImGui::Checkbox("Treat as CPU Skinned", treat_as_cpu_skinned);
    if (ImGui::IsItemHovered())
    {
      ImGui::SetItemTooltip(
          "Treat as CPU Skinned - at runtime Dolphin will determine the mesh movement and apply to "
          "a new mesh.  This incurs a runtime cost but allows for replacement of animated meshes.  "
          "Not necessary for GPU skinned meshes.");
    }

    if (ImGui::Button("Import", ImVec2(120, 0)))
    {
      ImportMeshData(filename, *components_to_copy, *treat_as_cpu_skinned);
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
