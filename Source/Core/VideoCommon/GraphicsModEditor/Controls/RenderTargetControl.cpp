// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/RenderTargetControl.h"

#include <chrono>

#include <imgui.h>

#include "Core/System.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/RenderTargetAsset.h"
#include "VideoCommon/GraphicsModEditor/Controls/TextureControl.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/Resources/CustomResourceManager.h"

namespace GraphicsModEditor::Controls
{
RenderTargetControl::RenderTargetControl(EditorState& state) : m_state(state)
{
}
void RenderTargetControl::DrawImGui(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                    VideoCommon::RenderTargetData* render_target_data,
                                    const std::filesystem::path& path,
                                    VideoCommon::CustomAsset::TimeType* last_data_write,
                                    AbstractTexture*)
{
  if (ImGui::BeginTable("RenderTargetForm", 2))
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
    ImGui::Text("Min Filter Mode");
    ImGui::TableNextColumn();
    if (ImGui::BeginCombo("##MinFilterMode",
                          fmt::to_string(render_target_data->sampler.tm0.min_filter).c_str()))
    {
      for (auto e = FilterMode::Near; e <= FilterMode::Linear;
           e = static_cast<FilterMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = render_target_data->sampler.tm0.min_filter == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          render_target_data->sampler.tm0.min_filter = e;
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
                          fmt::to_string(render_target_data->sampler.tm0.mag_filter).c_str()))
    {
      for (auto e = FilterMode::Near; e <= FilterMode::Linear;
           e = static_cast<FilterMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = render_target_data->sampler.tm0.mag_filter == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          render_target_data->sampler.tm0.mag_filter = e;
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
                          fmt::to_string(render_target_data->sampler.tm0.mipmap_filter).c_str()))
    {
      for (auto e = FilterMode::Near; e <= FilterMode::Linear;
           e = static_cast<FilterMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = render_target_data->sampler.tm0.mipmap_filter == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          render_target_data->sampler.tm0.mipmap_filter = e;
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
    if (ImGui::BeginCombo("##UWrapMode",
                          fmt::to_string(render_target_data->sampler.tm0.wrap_u).c_str()))
    {
      for (auto e = WrapMode::Clamp; e <= WrapMode::Mirror;
           e = static_cast<WrapMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = render_target_data->sampler.tm0.wrap_u == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          render_target_data->sampler.tm0.wrap_u = e;
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
    if (ImGui::BeginCombo("##VWrapMode",
                          fmt::to_string(render_target_data->sampler.tm0.wrap_v).c_str()))
    {
      for (auto e = WrapMode::Clamp; e <= WrapMode::Mirror;
           e = static_cast<WrapMode>(static_cast<u32>(e) + 1))
      {
        const bool is_selected = render_target_data->sampler.tm0.wrap_v == e;
        if (ImGui::Selectable(fmt::to_string(e).c_str(), is_selected))
        {
          render_target_data->sampler.tm0.wrap_v = e;
          *last_data_write = VideoCommon::CustomAsset::ClockType::now();
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Render Type");
    ImGui::TableNextColumn();
    if (ImGui::BeginCombo("##RenderType", fmt::to_string(render_target_data->render_type).c_str()))
    {
      static constexpr std::array<VideoCommon::RenderTargetData::RenderType, 2> all_render_types = {
          VideoCommon::RenderTargetData::RenderType::ScreenColor,
          VideoCommon::RenderTargetData::RenderType::ScreenDepth};

      for (const auto& render_type : all_render_types)
      {
        const bool is_selected = render_type == render_target_data->render_type;
        const auto render_type_str = fmt::to_string(render_type);
        if (ImGui::Selectable(fmt::format("{}##", render_type_str).c_str(), is_selected))
        {
          render_target_data->render_type = render_type;
          *last_data_write = VideoCommon::CustomAsset::ClockType::now();
          GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
        }
      }
      ImGui::EndCombo();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Width");
    ImGui::TableNextColumn();
    int width = render_target_data->width;
    if (ImGui::InputInt("##Width", &width))
    {
      render_target_data->width = static_cast<u16>(width);
      *last_data_write = VideoCommon::CustomAsset::ClockType::now();
      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Height");
    ImGui::TableNextColumn();
    int height = render_target_data->height;
    if (ImGui::InputInt("##Height", &height))
    {
      render_target_data->height = static_cast<u16>(height);
      *last_data_write = VideoCommon::CustomAsset::ClockType::now();
      GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Clear Value");
    ImGui::TableNextColumn();
    auto& clear_color = render_target_data->clear_color;
    if (render_target_data->render_type == VideoCommon::RenderTargetData::RenderType::ScreenColor)
    {
      if (ImGui::ColorEdit3("##Color", clear_color.data()))
      {
        *last_data_write = VideoCommon::CustomAsset::ClockType::now();
        GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
      }
    }
    else if (render_target_data->render_type ==
             VideoCommon::RenderTargetData::RenderType::ScreenDepth)
    {
      if (ImGui::DragFloat("##Value", &clear_color[0], 1.0f, 0.0f, 255.0f))
      {
        *last_data_write = VideoCommon::CustomAsset::ClockType::now();
        GraphicsModEditor::GetEditorEvents().asset_reload_event.Trigger(asset_id);
      }
    }
    ImGui::EndTable();

    auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();
    auto render_target_resource =
        resource_manager.GetRenderTargetFromAsset(asset_id, m_state.m_user_data.m_asset_library);
    auto render_target_resource_data = render_target_resource->GetData();
    if (render_target_resource_data)
    {
      auto* texture = render_target_resource_data->GetTexture();
      if (!texture)
        return;

      const float column_width = ImGui::GetContentRegionAvail().x;
      float image_width = texture->GetWidth();
      float image_height = texture->GetHeight();
      const float image_aspect_ratio = image_width / image_height;

      const float final_width = std::min(image_width * 4, column_width);
      image_width = final_width;
      image_height = final_width / image_aspect_ratio;
      ImGui::Image(*texture, ImVec2{image_width, image_height});
    }
  }
}
}  // namespace GraphicsModEditor::Controls
