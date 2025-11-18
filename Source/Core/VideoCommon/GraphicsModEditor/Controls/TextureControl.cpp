// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/TextureControl.h"

#include <chrono>

#include <imgui.h>

#include "Common/StringUtil.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"

namespace GraphicsModEditor::Controls
{
TextureControl::TextureControl(EditorState& state) : m_state(state)
{
}
void TextureControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                               VideoCommon::TextureAndSamplerData* texture_data,
                               const std::filesystem::path& path,
                               VideoCommon::CustomAsset::TimeType* last_data_write,
                               AbstractTexture* texture_preview)
{
  if (ImGui::BeginTable("TextureForm", 2))
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

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Type");
    ImGui::TableNextColumn();
    if (ImGui::BeginCombo("##TextureType", fmt::to_string(texture_data->type).c_str()))
    {
      static constexpr std::array<AbstractTextureType, 3> all_texture_types = {
          AbstractTextureType::Texture_2D, AbstractTextureType::Texture_CubeMap};

      for (const auto& texture_type : all_texture_types)
      {
        const bool is_selected = texture_type == texture_data->type;
        const auto texture_type_str = fmt::to_string(texture_type);
        if (ImGui::Selectable(fmt::format("{}##", texture_type_str).c_str(), is_selected))
        {
          texture_data->type = texture_type;
          *last_data_write = VideoCommon::CustomAsset::ClockType::now();
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Min Filter Mode");
    ImGui::TableNextColumn();
    if (ImGui::BeginCombo("##MinFilterMode",
                          fmt::to_string(texture_data->sampler.tm0.min_filter).c_str()))
    {
      for (auto e = FilterMode::Near; e <= FilterMode::Linear;
           e = static_cast<FilterMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = texture_data->sampler.tm0.min_filter == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          texture_data->sampler.tm0.min_filter = e;
          *last_data_write = VideoCommon::CustomAsset::ClockType::now();
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Mag Filter Mode");
    ImGui::TableNextColumn();
    if (ImGui::BeginCombo("##MagFilterMode",
                          fmt::to_string(texture_data->sampler.tm0.mag_filter).c_str()))
    {
      for (auto e = FilterMode::Near; e <= FilterMode::Linear;
           e = static_cast<FilterMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = texture_data->sampler.tm0.mag_filter == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          texture_data->sampler.tm0.mag_filter = e;
          *last_data_write = VideoCommon::CustomAsset::ClockType::now();
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Mip Filter Mode");
    ImGui::TableNextColumn();
    if (ImGui::BeginCombo("##MipFilterMode",
                          fmt::to_string(texture_data->sampler.tm0.mipmap_filter).c_str()))
    {
      for (auto e = FilterMode::Near; e <= FilterMode::Linear;
           e = static_cast<FilterMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = texture_data->sampler.tm0.mipmap_filter == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          texture_data->sampler.tm0.mipmap_filter = e;
          *last_data_write = VideoCommon::CustomAsset::ClockType::now();
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("U Wrap Mode");
    ImGui::TableNextColumn();
    if (ImGui::BeginCombo("##UWrapMode", fmt::to_string(texture_data->sampler.tm0.wrap_u).c_str()))
    {
      for (auto e = WrapMode::Clamp; e <= WrapMode::Mirror;
           e = static_cast<WrapMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = texture_data->sampler.tm0.wrap_u == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          texture_data->sampler.tm0.wrap_u = e;
          *last_data_write = VideoCommon::CustomAsset::ClockType::now();
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("V Wrap Mode");
    ImGui::TableNextColumn();
    if (ImGui::BeginCombo("##VWrapMode", fmt::to_string(texture_data->sampler.tm0.wrap_v).c_str()))
    {
      for (auto e = WrapMode::Clamp; e <= WrapMode::Mirror;
           e = static_cast<WrapMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = texture_data->sampler.tm0.wrap_v == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          texture_data->sampler.tm0.wrap_v = e;
          *last_data_write = VideoCommon::CustomAsset::ClockType::now();
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }
    ImGui::EndTable();

    if (texture_preview)
    {
      const auto ratio = texture_preview->GetWidth() / texture_preview->GetHeight();
      ImGui::Image(*texture_preview, ImVec2{ImGui::GetContentRegionAvail().x,
                                            ImGui::GetContentRegionAvail().x * ratio});
    }
  }
}
}  // namespace GraphicsModEditor::Controls
