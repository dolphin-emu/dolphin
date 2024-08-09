// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Panels/AssetBrowserPanel.h"

#include <filesystem>
#include <variant>

#include <fmt/format.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"
#include "Core/System.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/GraphicsModEditor/Controls/MaterialGenerateWindow.h"
#include "VideoCommon/GraphicsModEditor/Controls/MeshImportWindow.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/Present.h"

namespace
{
std::string FindNewName(const std::string& base_name, const std::string& extension,
                        const std::filesystem::path& base_path)
{
  u32 index = 1;
  std::string new_name = base_name + extension;
  while (std::filesystem::exists(base_path / new_name))
  {
    new_name = fmt::format("{} {}{}", base_name, index, extension);
    index++;
  }

  return new_name;
}
}  // namespace

namespace GraphicsModEditor::Panels
{
AssetBrowserPanel::AssetBrowserPanel(EditorState& state) : m_state(state)
{
  ResetCurrentPath();

  m_browse_event = EditorEvents::JumpToAssetInBrowserEvent::Register(
      [this](const VideoCommon::CustomAssetLibrary::AssetID& asset_id) { BrowseEvent(asset_id); },
      "EditorAssetBrowserPanel");
}

void AssetBrowserPanel::ResetCurrentPath()
{
  m_current_path = m_state.m_user_data.m_current_mod_path;
}

void AssetBrowserPanel::DrawImGui()
{
  const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
  u32 default_window_height = g_presenter->GetTargetRectangle().GetHeight() * 0.2;
  u32 default_window_width = g_presenter->GetTargetRectangle().GetWidth() * 0.95;
  ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + default_window_width * 0.1,
                                 main_viewport->WorkPos.y +
                                     g_presenter->GetTargetRectangle().GetHeight() -
                                     default_window_height),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(default_window_width, default_window_height),
                           ImGuiCond_FirstUseEver);

  ImGui::Begin("Asset Browser Panel");

  std::error_code ec;
  auto dir_iter = std::filesystem::directory_iterator(m_current_path, ec);
  if (ec)
  {
    ImGui::Text(
        "%s",
        fmt::format("Error viewing current directory '{}'", PathToString(m_current_path)).c_str());
    ImGui::End();
    return;
  }
  std::vector<std::filesystem::path> directory_entries;
  for (const auto& entry : dir_iter)
  {
    directory_entries.push_back(entry.path());
  }

  // Split the current path into pieces from the root
  const auto relative_path =
      std::filesystem::relative(m_current_path, m_state.m_user_data.m_current_mod_path, ec);
  if (ec)
  {
    ImGui::Text("%s", fmt::format("Error getting relative path from directory '{}' to root '{}'",
                                  PathToString(m_current_path),
                                  PathToString(m_state.m_user_data.m_current_mod_path))
                          .c_str());
    ImGui::End();
    return;
  }

  std::filesystem::path full_path = m_state.m_user_data.m_current_mod_path;
  if (ImGui::Button("Assets"))
  {
    m_current_path = full_path;
  }
  if (relative_path != std::filesystem::path{"."})
  {
    ImGui::SameLine();
    ImGui::Text(" > ");

    const std::vector<std::filesystem::path> path_pieces(relative_path.begin(),
                                                         relative_path.end());
    for (const auto& piece : path_pieces)
    {
      ImGui::SameLine();
      full_path /= piece;
      if (ImGui::Button(PathToString(piece).c_str()))
      {
        m_current_path = full_path;
      }
      ImGui::SameLine();
      ImGui::Text(" > ");
    }
  }

  const float search_size = 200.0f;
  const float search_padding = 50.0f;
  if (ImGui::GetWindowSize().x > (search_size + search_padding))
  {
    const ImVec2 dummy_size =
        ImVec2(ImGui::GetWindowSize().x - search_size - search_padding, ImGui::GetTextLineHeight());
    ImGui::Dummy(dummy_size);
    ImGui::SameLine();
  }
  ImGui::SetNextItemWidth(search_size);
  ImGui::InputTextWithHint("##", "Search...", &m_filter_text);

  const u32 column_count =
      static_cast<u32>(ImGui::GetContentRegionAvail().x) / (thumbnail_size + padding);
  u32 columns_displayed = 0;

  if (ImGui::BeginTable("AsetBrowserContentTable", column_count))
  {
    ImGui::TableNextRow();

    for (const auto& entry : directory_entries)
    {
      if (std::filesystem::is_directory(entry))
      {
        ImGui::TableNextColumn();
        ImGui::BeginGroup();
        if (ImGui::ImageButton(PathToString(entry).c_str(),
                               m_state.m_editor_data.m_name_to_texture["folder"].get(),
                               thumbnail_imgui_size))
        {
          m_current_path = entry;
        }
        ImGui::Text("%s", PathToString(entry.filename()).c_str());
        ImGui::EndGroup();
        columns_displayed++;
        if (columns_displayed == column_count)
        {
          ImGui::TableNextRow();
          columns_displayed = 0;
        }
      }
    }

    for (const auto& entry : directory_entries)
    {
      if (std::filesystem::is_regular_file(entry))
      {
        const std::string filename = PathToString(entry.filename());
        if (!m_filter_text.empty() && filename.find(m_filter_text) == std::string::npos)
          continue;
        auto ext = entry.extension().string();
        Common::ToLower(&ext);
        if (ext == ".dds" || ext == ".png")
        {
          ImGui::TableNextColumn();
          ImGui::BeginGroup();
          HandleAsset(entry, m_state.m_editor_data.m_name_to_texture["image"].get(),
                      "TextureAsset");
          ImGui::EndGroup();
          columns_displayed++;
        }
        else if (ext == ".glsl")
        {
          ImGui::TableNextColumn();
          ImGui::BeginGroup();
          HandleAsset(entry, m_state.m_editor_data.m_name_to_texture["code"].get(), "ShaderAsset");
          ImGui::EndGroup();
          columns_displayed++;
        }
        else if (ext == ".dolmesh")
        {
          ImGui::TableNextColumn();
          ImGui::BeginGroup();
          HandleAsset(entry, m_state.m_editor_data.m_name_to_texture["file"].get(), "MeshAsset");
          ImGui::EndGroup();
          columns_displayed++;
        }
        else if (ext == ".gltf")
        {
          ImGui::TableNextColumn();
          ImGui::BeginGroup();
          ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
          ImGui::ImageButton(PathToString(entry).c_str(),
                             m_state.m_editor_data.m_name_to_texture["file"].get(),
                             thumbnail_imgui_size);
          ImGui::PopStyleVar();
          if (ImGui::BeginPopupContextItem())
          {
            if (ImGui::Selectable("Import"))
            {
              m_is_mesh_import_active = true;
              m_mesh_import_filename = WithUnifiedPathSeparators(PathToString(full_path / entry));
            }

            ImGui::EndPopup();
          }
          ImGui::OpenPopupOnItemClick(PathToString(entry).c_str(),
                                      ImGuiPopupFlags_MouseButtonRight);

          ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
          ImGui::TextWrapped("%s", PathToString(entry.stem()).c_str());
          ImGui::PopStyleVar();

          ImGui::EndGroup();
          columns_displayed++;
        }
        else if (ext == ".material")
        {
          ImGui::TableNextColumn();
          ImGui::BeginGroup();
          HandleAsset(entry, m_state.m_editor_data.m_name_to_texture["file"].get(),
                      "MaterialAsset");
          ImGui::EndGroup();
          columns_displayed++;
        }

        if (columns_displayed == column_count)
        {
          ImGui::TableNextRow();
          columns_displayed = 0;
        }
      }
    }

    if (m_is_mesh_import_active)
    {
      if (Controls::ShowMeshImportWindow(m_mesh_import_filename, &m_mesh_import_import_materials))
      {
        m_is_mesh_import_active = false;
        m_mesh_import_import_materials = false;
      }
    }

    if (m_is_generate_material_active)
    {
      if (Controls::ShowMaterialGenerateWindow(&m_material_generation_context))
      {
        m_is_generate_material_active = false;
      }
    }

    if (ImGui::BeginPopupContextWindow("AssetBrowserGenericContextMenu",
                                       ImGuiPopupFlags_NoOpenOverItems |
                                           ImGuiPopupFlags_MouseButtonRight |
                                           ImGuiPopupFlags_NoOpenOverExistingPopup))
    {
      if (ImGui::BeginMenu("Create"))
      {
        if (ImGui::MenuItem("Material"))
        {
          std::string name = FindNewName("New Material", ".material", m_current_path);
          const auto new_path = m_current_path / name;
          if (File::WriteStringToFile(PathToString(new_path),
                                      m_state.m_editor_data.m_name_to_template["material"]))
          {
            m_state.m_user_data.m_asset_library->AddAsset(new_path);
            auto asset = m_state.m_user_data.m_asset_library->GetAssetFromPath(new_path);
            if (asset)
            {
              EditorEvents::ItemsSelectedEvent::Trigger(std::set<SelectableType>{asset});
              BeginRename(asset->m_asset_id, PathToString(new_path.stem()));
            }
          }
        }
        if (ImGui::MenuItem("Shader"))
        {
          const auto new_json_path =
              m_current_path / FindNewName("New Shader", ".shader", m_current_path);
          if (File::WriteStringToFile(
                  PathToString(new_json_path),
                  m_state.m_editor_data.m_name_to_template["pixel_shader_metadata"]))
          {
            std::string glsl_name = FindNewName("New Shader", ".glsl", m_current_path);
            const auto new_glsl_path = m_current_path / glsl_name;
            if (File::WriteStringToFile(PathToString(new_glsl_path),
                                        m_state.m_editor_data.m_name_to_template["pixel_shader"]))
            {
              m_state.m_user_data.m_asset_library->AddAsset(new_glsl_path);
              auto asset = m_state.m_user_data.m_asset_library->GetAssetFromPath(new_glsl_path);
              if (asset)
              {
                EditorEvents::ItemsSelectedEvent::Trigger(std::set<SelectableType>{asset});
                BeginRename(asset->m_asset_id, PathToString(new_glsl_path.stem()));
              }
            }
          }
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Generate Materials From Textures..."))
      {
        m_material_generation_context = {};
        m_material_generation_context.state = &m_state;
        m_is_generate_material_active = true;
        m_material_generation_context.input_path = PathToString(m_current_path);
        m_material_generation_context.output_path =
            PathToString(m_state.m_user_data.m_current_mod_path / "materials");
      }
      ImGui::EndPopup();
    }

    const bool window_clicked = (!ImGui::IsAnyItemHovered() &&
                                 ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
                                 ImGui::IsMouseClicked(ImGuiMouseButton_Left));
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || window_clicked)
    {
      EndRename();
    }

    ImGui::EndTable();
  }

  if (directory_entries.size() == 0)
  {
    ImGui::Text("No items in folder");
  }

  ImGui::End();
}

void AssetBrowserPanel::HandleAsset(const std::filesystem::path& asset_path,
                                    AbstractTexture* icon_texture, std::string_view drag_drop_name)
{
  EditorAsset* asset = m_state.m_user_data.m_asset_library->GetAssetFromPath(asset_path);
  if (asset)
  {
    AbstractTexture* texture =
        m_state.m_user_data.m_asset_library->GetAssetPreview(asset->m_asset_id);
    m_state.m_editor_data.m_assets_waiting_for_preview.erase(asset->m_asset_id);
    if (!texture)
      texture = icon_texture;

    if (ImGui::ImageButton(PathToString(asset_path).c_str(), texture, thumbnail_imgui_size))
    {
      EndRename();
      EditorEvents::ItemsSelectedEvent::Trigger(std::set<SelectableType>{asset});
    }
    if (ImGui::BeginPopupContextItem())
    {
      EndRename();
      if (ImGui::MenuItem("Remove from project"))
      {
        m_state.m_user_data.m_asset_library->RemoveAsset(asset_path);
        EditorEvents::ItemsSelectedEvent::Trigger(std::set<SelectableType>{});
      }
      if (ImGui::MenuItem("Rename"))
      {
        BeginRename(asset->m_asset_id, PathToString(asset_path.stem()));
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(PathToString(asset_path).c_str(), ImGuiPopupFlags_MouseButtonRight);
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
      ImGui::SetDragDropPayload(drag_drop_name.data(), asset->m_asset_id.c_str(),
                                asset->m_asset_id.size());

      ImGui::Text("%s", PathToString(asset->m_asset_path.stem()).c_str());
      ImGui::EndDragDropSource();
    }
    if (m_renamed_asset_id == asset->m_asset_id)
    {
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
      ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
      if (!m_is_rename_focused)
      {
        ImGui::SetKeyboardFocusHere();
        m_is_rename_focused = true;
      }
      if (ImGui::InputText("##AssetRename", &m_rename_text, ImGuiInputTextFlags_EnterReturnsTrue))
      {
        m_does_rename_have_error = false;

        const auto old_path = asset_path;
        const auto new_path =
            asset_path.parent_path() / (m_rename_text + PathToString(asset_path.extension()));
        if (m_state.m_user_data.m_asset_library->RenameAsset(old_path, new_path))
        {
          EndRename();
        }
        else
        {
          if (!m_does_rename_have_error)
          {
            ImGui::OpenPopup("##RenameHasError");

            // Focus seems to be stolen so tell input to focus again
            m_is_rename_focused = false;
          }
          m_does_rename_have_error = true;
        }
      }
      ImGui::PopStyleColor();
      ImGui::PopStyleVar();
      ImGui::PopStyleVar();

      ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMax().x + 10, ImGui::GetItemRectMin().y));
      if (ImGui::BeginPopup("##RenameHasError",
                            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_ChildWindow))
      {
        const ImVec4 red_tint(1, 0, 0, 1);
        ImGui::Image(m_state.m_editor_data.m_name_to_texture["error"].get(), ImVec2(25, 25),
                     ImVec2(0, 0), ImVec2(1, 1), red_tint);
        ImGui::SameLine();
        ImGui::Text("File already exists, choose a different name");
        if (!m_does_rename_have_error)
        {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
    else
    {
      std::string asset_path_filename = PathToString(asset_path.stem());
      ImGui::TextWrapped("%s", asset_path_filename.c_str());
      if (ImGui::IsItemHovered())
      {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
          BeginRename(asset->m_asset_id, std::move(asset_path_filename));
      }
    }
  }
  else
  {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
    if (ImGui::ImageButton(PathToString(asset_path).c_str(), icon_texture, thumbnail_imgui_size))
    {
      EndRename();
    }
    ImGui::PopStyleVar();
    if (ImGui::BeginPopupContextItem())
    {
      EndRename();
      if (ImGui::MenuItem("Add to project"))
      {
        m_state.m_user_data.m_asset_library->AddAsset(asset_path);
        GraphicsModEditor::EditorEvents::ChangeOccurredEvent::Trigger();

        asset = m_state.m_user_data.m_asset_library->GetAssetFromPath(asset_path);
        if (asset)
        {
          auto& system = Core::System::GetInstance();
          auto& loader = system.GetCustomAssetLoader();
          // TODO: generate previews for other assets...
          if (asset->m_asset_map.find("texture") != asset->m_asset_map.end())
          {
            m_state.m_editor_data.m_assets_waiting_for_preview.try_emplace(
                asset->m_asset_id,
                loader.LoadGameTexture(asset->m_asset_id, m_state.m_user_data.m_asset_library));
          }
        }
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(PathToString(asset_path).c_str(), ImGuiPopupFlags_MouseButtonRight);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
    ImGui::TextWrapped("%s", PathToString(asset_path.stem()).c_str());
    ImGui::PopStyleVar();
  }
}

void AssetBrowserPanel::BeginRename(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                    std::string rename_text)
{
  m_renamed_asset_id = asset_id;
  m_rename_text = std::move(rename_text);
  m_is_rename_focused = false;
}

void AssetBrowserPanel::EndRename()
{
  m_renamed_asset_id.reset();
  m_rename_text = "";
}

void AssetBrowserPanel::BrowseEvent(const VideoCommon::CustomAssetLibrary::AssetID& asset_id)
{
  EditorAsset* asset = m_state.m_user_data.m_asset_library->GetAssetFromID(asset_id);
  if (asset)
  {
    m_current_path = asset->m_asset_path.parent_path();
    EditorEvents::ItemsSelectedEvent::Trigger(std::set<SelectableType>{asset});
  }
}
}  // namespace GraphicsModEditor::Panels
