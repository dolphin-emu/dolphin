// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"

#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

#include <string_view>

#include <imgui.h>

namespace GraphicsModEditor::Controls
{
namespace
{
std::string_view AssetDragDropTypeFromType(AssetDataType asset_type)
{
  switch (asset_type)
  {
  case Material:
    return "MaterialAsset";
  case PixelShader:
    return "ShaderAsset";
  case Texture:
    return "TextureAsset";
  case Mesh:
    return "MeshAsset";
  }

  return "";
}

AbstractTexture* GenericImageIconFromType(AssetDataType asset_type, const EditorState& state)
{
  switch (asset_type)
  {
  case Material:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("file");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
  case PixelShader:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("code");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
  case Texture:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("image");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
  case Mesh:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("file");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
  }

  return nullptr;
}

ImVec2 asset_button_size{150, 150};
}  // namespace
bool AssetDisplay(std::string_view popup_name, EditorState* state,
                  VideoCommon::CustomAssetLibrary::AssetID* asset_id, AssetDataType asset_type)
{
  if (!state) [[unlikely]]
    return false;
  if (!asset_id) [[unlikely]]
    return false;

  static std::string asset_filter_text = "";

  const std::string reset_popup_name = std::string{popup_name} + "Reset";
  bool changed = false;
  const EditorAsset* asset = nullptr;
  if (!asset_id->empty())
  {
    asset = state->m_user_data.m_asset_library->GetAssetFromID(*asset_id);
  }
  if (!asset)
  {
    if (ImGui::Button("None", asset_button_size))
    {
      if (!ImGui::IsPopupOpen(popup_name.data()))
      {
        asset_filter_text = "";
        ImGui::OpenPopup(popup_name.data());
      }
    }
  }
  else
  {
    AbstractTexture* texture =
        state->m_user_data.m_asset_library->GetAssetPreview(asset->m_asset_id);
    state->m_editor_data.m_assets_waiting_for_preview.erase(asset->m_asset_id);
    if (!texture)
      texture = GenericImageIconFromType(asset_type, *state);
    ImGui::BeginGroup();
    if (texture)
    {
      if (ImGui::ImageButton(asset->m_asset_id.c_str(), texture, asset_button_size))
      {
        if (!ImGui::IsPopupOpen(popup_name.data()))
        {
          asset_filter_text = "";
          ImGui::OpenPopup(popup_name.data());
        }
      }
      if (ImGui::BeginPopupContextItem(reset_popup_name.data()))
      {
        if (ImGui::Selectable("View properties"))
        {
          EditorEvents::JumpToAssetInBrowserEvent::Trigger(asset->m_asset_id);
        }
        if (ImGui::Selectable("Reset"))
        {
          *asset_id = "";
          changed = true;
        }
        ImGui::EndPopup();
      }
      ImGui::TextWrapped("%s", PathToString(asset->m_asset_path.stem()).c_str());
    }
    else
    {
      if (ImGui::Button(PathToString(asset->m_asset_path).c_str(), asset_button_size))
      {
        if (!ImGui::IsPopupOpen(popup_name.data()))
        {
          asset_filter_text = "";
          ImGui::OpenPopup(popup_name.data());
        }
      }
    }
    ImGui::EndGroup();
  }
  if (ImGui::BeginDragDropTarget())
  {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(AssetDragDropTypeFromType(asset_type).data()))
    {
      VideoCommon::CustomAssetLibrary::AssetID new_asset_id(static_cast<const char*>(payload->Data),
                                                            payload->DataSize);
      *asset_id = new_asset_id;
      changed = true;
    }
    ImGui::EndDragDropTarget();
  }

  // Asset browser popup below
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  const ImVec2 size = ImGui::GetMainViewport()->WorkSize;
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(size.x / 4.0f, size.y / 2.0f));
  if (ImGui::BeginPopup(popup_name.data()))
  {
    const u32 column_count = 5;
    u32 current_columns = 0;
    u32 assets_displayed = 0;

    const float search_size = 200.0f;
    ImGui::SetNextItemWidth(search_size);
    ImGui::InputTextWithHint("##", "Search...", &asset_filter_text);

    if (ImGui::BeginTable("AssetBrowserPopupTable", column_count))
    {
      ImGui::TableNextRow();
      for (const auto& asset_from_library : state->m_user_data.m_asset_library->GetAllAssets())
      {
        if (asset_from_library->m_data_type != asset_type)
          continue;

        const auto name = PathToString(asset_from_library->m_asset_path.stem());
        if (!asset_filter_text.empty() && name.find(asset_filter_text) == std::string::npos)
        {
          continue;
        }

        assets_displayed++;
        ImGui::TableNextColumn();
        ImGui::BeginGroup();

        AbstractTexture* texture =
            state->m_user_data.m_asset_library->GetAssetPreview(asset_from_library->m_asset_id);
        if (!texture)
          texture = GenericImageIconFromType(asset_type, *state);
        if (texture)
        {
          if (ImGui::ImageButton(asset_from_library->m_asset_id.c_str(), texture,
                                 asset_button_size))
          {
            *asset_id = asset_from_library->m_asset_id;
            changed = true;
            ImGui::CloseCurrentPopup();
          }
          ImGui::TextWrapped("%s", name.c_str());
        }
        else
        {
          if (ImGui::Button(name.c_str(), asset_button_size))
          {
            *asset_id = asset_from_library->m_asset_id;
            changed = true;
            ImGui::CloseCurrentPopup();
          }
        }
        ImGui::EndGroup();

        current_columns++;
        if (current_columns == column_count)
        {
          ImGui::TableNextRow();
          current_columns = 0;
        }
      }
      ImGui::EndTable();
    }

    if (assets_displayed == 0)
    {
      ImGui::Text("No assets found");
    }
    ImGui::EndPopup();
  }

  return changed;
}
}  // namespace GraphicsModEditor::Controls
