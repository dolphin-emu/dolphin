// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"

#include <algorithm>
#include <array>
#include <string_view>

#include <imgui.h>

#include "Core/System.h"

#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/Resources/CustomResourceManager.h"
#include "VideoCommon/Resources/RenderTargetResource.h"

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
    return "PixelShaderAsset";
  case Shader:
    return "ShaderAsset";
  case Texture:
    return "TextureAsset";
  case Mesh:
    return "MeshAsset";
  case RenderTarget:
    return "RenderTargetAsset";
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
    break;
  case PixelShader:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("code");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
    break;
  case Shader:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("code");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
    break;
  case Texture:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("image");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
    break;
  case Mesh:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("file");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
    break;
  case RenderTarget:
    if (const auto it = state.m_editor_data.m_name_to_texture.find("image");
        it != state.m_editor_data.m_name_to_texture.end())
    {
      return it->second.get();
    }
    break;
  }

  return nullptr;
}

AbstractTexture* GetTexturePreview(AssetDataType asset_type,
                                   const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                   EditorState* state)
{
  if (asset_type == AssetDataType::RenderTarget)
  {
    auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();
    const auto render_target_resource =
        resource_manager.GetRenderTargetFromAsset(asset_id, state->m_user_data.m_asset_library);
    const auto data = render_target_resource->GetData();
    if (data)
    {
      return data->GetTexture();
    }
    return nullptr;
  }
  else
  {
    AbstractTexture* texture = state->m_user_data.m_asset_library->GetAssetPreview(asset_id);
    state->m_editor_data.m_assets_waiting_for_preview.erase(asset_id);
    return texture;
  }
}

ImVec2 asset_button_size{150, 150};
}  // namespace
bool AssetDisplay(std::string_view name, EditorState* state,
                  VideoCommon::CustomAssetLibrary::AssetID* asset_id_chosen,
                  AssetDataType* asset_type_chosen,
                  std::span<const AssetDataType> asset_types_allowed)
{
  if (!state) [[unlikely]]
    return false;
  if (!asset_id_chosen) [[unlikely]]
    return false;
  if (!asset_type_chosen) [[unlikely]]
    return false;

  static std::string asset_filter_text = "";

  const std::string reset_popup_name = std::string{name} + "Reset";
  bool changed = false;
  const EditorAsset* asset = nullptr;
  if (!asset_id_chosen->empty())
  {
    asset = state->m_user_data.m_asset_library->GetAssetFromID(*asset_id_chosen);
  }
  if (!asset)
  {
    if (ImGui::Button(fmt::format("None##{}", name).c_str(), asset_button_size))
    {
      if (!ImGui::IsPopupOpen(name.data()))
      {
        asset_filter_text = "";
        ImGui::OpenPopup(name.data());
      }
    }
  }
  else
  {
    const auto filename = PathToString(asset->m_asset_path.stem());
    AbstractTexture* texture = GetTexturePreview(*asset_type_chosen, asset->m_asset_id, state);
    ImGui::BeginGroup();
    if (texture)
    {
      if (ImGui::ImageButton(filename.c_str(), *texture, asset_button_size))
      {
        if (!ImGui::IsPopupOpen(name.data()))
        {
          asset_filter_text = "";
          ImGui::OpenPopup(name.data());
        }
      }
      if (ImGui::BeginPopupContextItem(reset_popup_name.data()))
      {
        if (ImGui::Selectable("View properties"))
        {
          GetEditorEvents().jump_to_asset_in_browser_event.Trigger(asset->m_asset_id);
        }
        if (ImGui::Selectable("Reset"))
        {
          *asset_id_chosen = "";
          changed = true;
        }
        ImGui::EndPopup();
      }
      ImGui::TextWrapped("%s", filename.c_str());
    }
    else
    {
      if (ImGui::Button(fmt::format("{}##{}", filename, name).c_str(), asset_button_size))
      {
        if (!ImGui::IsPopupOpen(name.data()))
        {
          asset_filter_text = "";
          ImGui::OpenPopup(name.data());
        }
      }
      ImGui::TextWrapped("%s", filename.c_str());
    }
    ImGui::EndGroup();
  }
  if (ImGui::BeginDragDropTarget())
  {
    for (const auto asset_type_allowed : asset_types_allowed)
    {
      if (const ImGuiPayload* payload =
              ImGui::AcceptDragDropPayload(AssetDragDropTypeFromType(asset_type_allowed).data()))
      {
        VideoCommon::CustomAssetLibrary::AssetID new_asset_id(
            static_cast<const char*>(payload->Data), payload->DataSize);
        *asset_id_chosen = new_asset_id;
        *asset_type_chosen = asset_type_allowed;
        changed = true;
      }
    }
    ImGui::EndDragDropTarget();
  }

  // Asset browser popup below
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  const ImVec2 size = ImGui::GetMainViewport()->WorkSize;
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(size.x / 4.0f, size.y / 2.0f));
  if (ImGui::BeginPopup(name.data()))
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
        if (std::ranges::find(asset_types_allowed, asset_from_library.m_data_type) ==
            asset_types_allowed.end())
        {
          continue;
        }

        const auto filename = PathToString(asset_from_library.m_asset_path.stem());
        if (!asset_filter_text.empty() && filename.find(asset_filter_text) == std::string::npos)
        {
          continue;
        }

        assets_displayed++;
        ImGui::TableNextColumn();
        ImGui::BeginGroup();

        AbstractTexture* texture =
            GetTexturePreview(asset_from_library.m_data_type, asset_from_library.m_asset_id, state);
        if (texture)
        {
          if (ImGui::ImageButton(asset_from_library.m_asset_id.c_str(), *texture,
                                 asset_button_size))
          {
            *asset_id_chosen = asset_from_library.m_asset_id;
            changed = true;
            ImGui::CloseCurrentPopup();
          }
          ImGui::TextWrapped("%s", filename.c_str());
        }
        else
        {
          if (ImGui::Button(fmt::format("{}##{}", filename, asset_from_library.m_asset_id).c_str(),
                            asset_button_size))
          {
            *asset_id_chosen = asset_from_library.m_asset_id;
            changed = true;
            ImGui::CloseCurrentPopup();
          }
          ImGui::SetItemTooltip("%s", filename.c_str());
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

bool AssetDisplay(std::string_view name, EditorState* state,
                  VideoCommon::CustomAssetLibrary::AssetID* asset_id_chosen,
                  AssetDataType asset_type_allowed)
{
  AssetDataType asset_data_type_chosen;
  return AssetDisplay(name, state, asset_id_chosen, &asset_data_type_chosen,
                      std::array<AssetDataType, 1>{asset_type_allowed});
}
}  // namespace GraphicsModEditor::Controls
