// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Panels/PropertiesPanel.h"

#include <filesystem>

#include <fmt/format.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/EnumUtils.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/TextureUtils.h"

namespace GraphicsModEditor::Panels
{
PropertiesPanel::PropertiesPanel(EditorState& state)
    : m_state(state), m_material_control(m_state), m_shader_control(m_state),
      m_texture_control(m_state), m_mesh_control(m_state)
{
  m_selection_event = EditorEvents::ItemsSelectedEvent::Register(
      [this](const auto& selected_targets) { SelectionOccurred(selected_targets); },
      "EditorPropertiesPanel");
}

void PropertiesPanel::DrawImGui()
{
  const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
  u32 default_window_height = g_presenter->GetTargetRectangle().GetHeight() -
                              ((float)g_presenter->GetTargetRectangle().GetHeight() * 0.1);
  u32 default_window_width = ((float)g_presenter->GetTargetRectangle().GetWidth() * 0.15);
  ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x +
                                     g_presenter->GetTargetRectangle().GetWidth() -
                                     default_window_width * 1.25,
                                 main_viewport->WorkPos.y +
                                     ((float)g_presenter->GetTargetRectangle().GetHeight() * 0.05)),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(default_window_width, default_window_height),
                           ImGuiCond_FirstUseEver);

  ImGui::Begin("Properties Panel");

  if (m_selected_targets.size() > 1)
  {
    ImGui::Text("Multiple objects not yet supported");
  }
  else if (m_selected_targets.size() == 1)
  {
    std::visit(overloaded{[&](const GraphicsMods::DrawCallID& drawcallid) {
                            DrawCallIDSelected(drawcallid);
                          },
                          [&](const FBInfo& fbid) { FBCallIDSelected(fbid); },
                          [&](const GraphicsMods::LightID& light_id) { LightSelected(light_id); },
                          [&](GraphicsModAction* action) { action->DrawImGui(); },
                          [&](EditorAsset* asset_data) { AssetDataSelected(asset_data); }},
               *m_selected_targets.begin());
  }
  ImGui::End();
}

void PropertiesPanel::DrawCallIDSelected(const GraphicsMods::DrawCallID& selected_object)
{
  const auto& data = m_state.m_runtime_data.m_draw_call_id_to_data[selected_object];
  auto& user_data = m_state.m_user_data.m_draw_call_id_to_user_data[selected_object];

  if (ImGui::BeginTable("DrawCallBasicForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("DisplayName");
    ImGui::TableNextColumn();
    ImGui::InputText("##FrameTargetDisplayName", &user_data.m_friendly_name);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ID");
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", fmt::to_string(Common::ToUnderlying(selected_object)).c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Time Created");
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s",
                       fmt::format("{}", data.m_create_time.time_since_epoch().count()).c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Projection Type");
    ImGui::TableNextColumn();
    ImGui::Text("%s", fmt::format("{}", data.m_projection_type).c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Cull Mode");
    ImGui::TableNextColumn();
    ImGui::Text("%s", fmt::format("{}", data.m_cull_mode).c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("World Position");
    ImGui::TableNextColumn();
    std::array<float, 4> test = data.m_world_position;
    ImGui::InputFloat4("##BooYah", &test[0]);

    ImGui::EndTable();
  }

  if (ImGui::CollapsingHeader("Blending", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("DrawBlendingForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Blend enabled?");
      ImGui::TableNextColumn();
      if (data.m_blendenable)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Color update enabled?");
      ImGui::TableNextColumn();
      if (data.m_colorupdate)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Logicop update enabled?");
      ImGui::TableNextColumn();
      if (data.m_logicopenable)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Destination factor");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.m_dstfactor).c_str());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Source factor");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.m_srcfactor).c_str());

      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("DrawTexturesForm", 2))
    {
      for (const auto& texture_details : data.m_textures_details)
      {
        if (texture_details.m_texture)
        {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Texture (%i)", texture_details.m_texture_unit);
          ImGui::TableNextColumn();

          const float column_width = ImGui::GetContentRegionAvail().x;
          float image_width = texture_details.m_texture->GetWidth();
          float image_height = texture_details.m_texture->GetHeight();
          const float image_aspect_ratio = image_width / image_height;

          const float final_width = std::min(image_width * 4, column_width);
          image_width = final_width;
          image_height = final_width / image_aspect_ratio;

          ImGui::ImageButton(texture_details.m_hash.data(), texture_details.m_texture,
                             ImVec2{image_width, image_height});
          if (ImGui::BeginPopupContextItem())
          {
            if (ImGui::Selectable("Dump"))
            {
              VideoCommon::TextureUtils::DumpTexture(*texture_details.m_texture,
                                                     std::string{texture_details.m_hash}, 0, false);
            }
            if (ImGui::Selectable("Copy hash"))
            {
              ImGui::SetClipboardText(texture_details.m_hash.data());
            }
            ImGui::EndPopup();
          }
          ImGui::Text("%dx%d", texture_details.m_texture->GetWidth(),
                      texture_details.m_texture->GetHeight());
        }
      }
      ImGui::EndTable();
    }
  }
}

void PropertiesPanel::FBCallIDSelected(const FBInfo& selected_object)
{
  const auto& data = m_state.m_runtime_data.m_fb_call_id_to_data[selected_object];
  auto& user_data = m_state.m_user_data.m_fb_call_id_to_user_data[selected_object];

  if (ImGui::BeginTable("FBTargetForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("DisplayName");
    ImGui::TableNextColumn();
    ImGui::InputText("##FBTargetDisplayName", &user_data.m_friendly_name);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ID");
    ImGui::TableNextColumn();
    ImGui::Text("%s", fmt::format("{}", selected_object.CalculateHash()).c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Time Created");
    ImGui::TableNextColumn();
    ImGui::Text("%s", fmt::format("{}", data.m_time.time_since_epoch().count()).c_str());

    if (data.m_texture)
    {
      const float column_width = ImGui::GetContentRegionAvail().x;
      float image_width = data.m_texture->GetWidth();
      float image_height = data.m_texture->GetHeight();
      const float image_aspect_ratio = image_width / image_height;

      image_width = column_width;
      image_height = column_width * image_aspect_ratio;

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Texture");
      ImGui::TableNextColumn();
      ImGui::Image(data.m_texture, ImVec2{image_width, image_height});
    }

    ImGui::EndTable();
  }
}

void PropertiesPanel::LightSelected(const GraphicsMods::LightID& selected_object)
{
  auto& data = m_state.m_runtime_data.m_light_id_to_data[selected_object];
  auto& user_data = m_state.m_user_data.m_light_id_to_user_data[selected_object];

  if (ImGui::BeginTable("LightTargetForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("DisplayName");
    ImGui::TableNextColumn();
    ImGui::InputText("##LightTargetDisplayName", &user_data.m_friendly_name);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ID");
    ImGui::TableNextColumn();
    ImGui::Text("%s", fmt::to_string(Common::ToUnderlying(selected_object)).c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Time Created");
    ImGui::TableNextColumn();
    ImGui::Text("%s", fmt::format("{}", data.m_create_time.time_since_epoch().count()).c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Color");
    ImGui::TableNextColumn();
    ImGui::InputInt4("##LightColor", data.m_color.data(), ImGuiInputTextFlags_ReadOnly);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Position");
    ImGui::TableNextColumn();
    ImGui::InputFloat4("##LightPosition", data.m_pos.data(), "%.3f", ImGuiInputTextFlags_ReadOnly);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Direction");
    ImGui::TableNextColumn();
    ImGui::InputFloat4("##LightDirection", data.m_dir.data(), "%.3f", ImGuiInputTextFlags_ReadOnly);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Distance Attenutation");
    ImGui::TableNextColumn();
    ImGui::InputFloat4("##LightDistAtt", data.m_distatt.data(), "%.3f",
                       ImGuiInputTextFlags_ReadOnly);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Cosine Attenutation");
    ImGui::TableNextColumn();
    ImGui::InputFloat4("##LightCosAtt", data.m_cosatt.data(), "%.3f", ImGuiInputTextFlags_ReadOnly);

    ImGui::EndTable();
  }
}

void PropertiesPanel::AssetDataSelected(EditorAsset* selected_object)
{
  std::visit(
      overloaded{[&](const std::unique_ptr<VideoCommon::MaterialData>& material_data) {
                   m_material_control.DrawImGui(selected_object->m_asset_id, material_data.get(),
                                                &selected_object->m_last_data_write,
                                                &selected_object->m_valid);
                 },
                 [&](const std::unique_ptr<VideoCommon::PixelShaderData>& pixel_shader_data) {
                   m_shader_control.DrawImGui(selected_object->m_asset_id, pixel_shader_data.get(),
                                              &selected_object->m_last_data_write);
                 },
                 [&](const std::unique_ptr<VideoCommon::TextureData>& texture_data) {
                   auto asset_preview = m_state.m_user_data.m_asset_library->GetAssetPreview(
                       selected_object->m_asset_id);
                   m_texture_control.DrawImGui(selected_object->m_asset_id, texture_data.get(),
                                               selected_object->m_asset_path,
                                               &selected_object->m_last_data_write, asset_preview);
                 },
                 [&](const std::unique_ptr<VideoCommon::MeshData>& mesh_data) {
                   auto asset_preview = m_state.m_user_data.m_asset_library->GetAssetPreview(
                       selected_object->m_asset_id);
                   m_mesh_control.DrawImGui(selected_object->m_asset_id, mesh_data.get(),
                                            selected_object->m_asset_path,
                                            &selected_object->m_last_data_write, asset_preview);
                 }},
      selected_object->m_data);
}

void PropertiesPanel::SelectionOccurred(const std::set<SelectableType>& selection)
{
  m_selected_targets = selection;
}
}  // namespace GraphicsModEditor::Panels
