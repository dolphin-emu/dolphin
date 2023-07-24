// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Panels/PropertiesPanel.h"

#include <span>
#include <utility>

#include <fmt/format.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/GraphicsModEditor/Controls/MiscControls.h"
#include "VideoCommon/GraphicsModEditor/Controls/TagSelectionWindow.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModTags.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/TextureUtils.h"

namespace
{
template <typename EnumType>
void ShowTags(std::span<const EnumType> tag_enums,
              std::span<const GraphicsModSystem::Config::GraphicsModTag> tag_datas,
              Common::Flags<EnumType>* flags, bool* window_active)
{
  if (ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen))
  {
    std::size_t tag_count = 0;
    std::size_t tag_removed_count = 0;
    for (std::size_t i = 0; i < tag_enums.size(); i++)
    {
      const auto tag_enum = tag_enums[i];
      const auto& tag_data = tag_datas[i];

      if ((*flags)[tag_enum])
      {
        if (GraphicsModEditor::Controls::ColorButton(
                tag_data.m_name.c_str(), GraphicsModEditor::Controls::tag_size,
                ImVec4(tag_data.m_color.x, tag_data.m_color.y, tag_data.m_color.z, 1)))
        {
          (*flags)[tag_enum] = false;
          GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
          tag_removed_count++;
        }

        tag_count++;
      }
    }

    if (tag_count && tag_count > tag_removed_count)
    {
      ImGui::SameLine();
    }

    if (ImGui::SmallButton("+"))
    {
      *window_active = true;
    }

    if (*window_active)
    {
      std::size_t tag_index;
      if (GraphicsModEditor::Controls::TagSelectionWindow("TagSelectionWindow", tag_datas,
                                                          &tag_index))
      {
        (*flags)[tag_enums[tag_index]] = true;
        GraphicsModEditor::GetEditorEvents().change_occurred_event.Trigger();
        *window_active = false;
      }
    }
  }
}
}  // namespace

namespace GraphicsModEditor::Panels
{
PropertiesPanel::PropertiesPanel(EditorState& state)
    : m_state(state), m_material_control(m_state), m_mesh_control(m_state),
      m_render_target_control(m_state), m_shader_control(m_state), m_texture_control(m_state)
{
  m_selection_event = GetEditorEvents().items_selected_event.Register(
      [this](const auto& selected_targets) { SelectionOccurred(selected_targets); });
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
    std::visit(
        overloaded{[&](const GraphicsModSystem::DrawCallID& drawcallid) {
                     DrawCallIDSelected(drawcallid);
                   },
                   [&](const GraphicsModSystem::TextureCacheID& tcache_id) {
                     TextureCacheIDSelected(tcache_id);
                   },
                   [&](const GraphicsModSystem::LightID& light_id) { LightSelected(light_id); },
                   [&](GraphicsModAction* action) { action->DrawImGui(); },
                   [&](EditorAsset* asset_data) { AssetDataSelected(asset_data); }},
        *m_selected_targets.begin());
  }
  ImGui::End();
}

void PropertiesPanel::DrawCallIDSelected(const GraphicsModSystem::DrawCallID& selected_object)
{
  const auto& data = m_state.m_runtime_data.m_draw_call_id_to_data[selected_object];

  if (ImGui::BeginTable("DrawCallBasicForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("DisplayName");
    ImGui::TableNextColumn();

    std::string friendly_name;
    if (const auto user_iter =
            m_state.m_user_data.m_draw_call_id_to_user_data.find(selected_object);
        user_iter != m_state.m_user_data.m_draw_call_id_to_user_data.end())
    {
      friendly_name = user_iter->second.m_friendly_name;
    }
    if (ImGui::InputText("##FrameTargetDisplayName", &friendly_name))
    {
      auto& user_data = m_state.m_user_data.m_draw_call_id_to_user_data[selected_object];
      user_data.m_friendly_name = std::move(friendly_name);
      GetEditorEvents().change_occurred_event.Trigger();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ID");
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%llu", std::to_underlying(selected_object));

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Material ID");
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%llu", std::to_underlying(data.material_id));

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Mesh ID");
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%llu", std::to_underlying(data.mesh_id));

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Time Created");
    ImGui::TableNextColumn();
    ImGui::Text("%lld", static_cast<long long int>(data.create_time.time_since_epoch().count()));

    ImGui::EndTable();
  }

  if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("CameraForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Projection Type");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.draw_data.projection_type).c_str());

      static constexpr double epsilon = 1e-5;
      if (data.draw_data.projection_type == ProjectionType::Perspective)
      {
        if (data.draw_data.projection_mat[2] > epsilon)
        {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("FOV");
          ImGui::TableNextColumn();

          static constexpr double PI = 3.14159265358979323846;
          const double fov = 2.0 * atan(1.0 / data.draw_data.projection_mat[2]) * 180.0 / PI;
          ImGui::Text("%lf", fov);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Aspect Ratio");
        ImGui::TableNextColumn();
        const double aspect_ratio =
            data.draw_data.projection_mat[2] / data.draw_data.projection_mat[0];
        ImGui::Text("%lf", aspect_ratio);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Near");
        ImGui::TableNextColumn();
        const double near_plane =
            data.draw_data.projection_mat[5] / (data.draw_data.projection_mat[4] - 1.0);
        ImGui::Text("%lf", near_plane);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Far");
        ImGui::TableNextColumn();
        const double far_plane =
            data.draw_data.projection_mat[5] / (data.draw_data.projection_mat[4] + 1.0);
        ImGui::Text("%lf", far_plane);
      }

      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("TransformForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("GPU Skinning");
      ImGui::TableNextColumn();
      if (data.draw_data.is_gpu_skinning)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");
      ImGui::EndTable();
    }
  }

  if (const auto user_iter = m_state.m_user_data.m_draw_call_id_to_user_data.find(selected_object);
      user_iter != m_state.m_user_data.m_draw_call_id_to_user_data.end())
  {
    ShowTags<GraphicsModSystem::DrawCallTag>(GraphicsModSystem::Runtime::GetDrawCallTags(),
                                             GraphicsModSystem::Runtime::GetDrawCallTagConfig(),
                                             &user_iter->second.tag_flags,
                                             &m_tag_selection_window_active);
  }

  if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("DrawGeometryForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Index Count");
      ImGui::TableNextColumn();
      ImGui::Text("%zu", data.draw_data.index_count);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Vertex Count");
      ImGui::TableNextColumn();
      ImGui::Text("%zu", data.draw_data.vertex_count);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Cull Mode");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.draw_data.rasterization_state.cull_mode).c_str());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Primitive Type");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.draw_data.rasterization_state.primitive).c_str());

      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Blending", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("DrawBlendingForm", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Blend enabled?");
      ImGui::TableNextColumn();
      if (data.draw_data.blending_state.blend_enable)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Logicop update enabled?");
      ImGui::TableNextColumn();
      if (data.draw_data.blending_state.logic_op_enable)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Color update enabled?");
      ImGui::TableNextColumn();
      if (data.draw_data.blending_state.color_update)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Alpha update enabled?");
      ImGui::TableNextColumn();
      if (data.draw_data.blending_state.alpha_update)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Subtract set?");
      ImGui::TableNextColumn();
      if (data.draw_data.blending_state.subtract)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Subtract Alpha Set?");
      ImGui::TableNextColumn();
      if (data.draw_data.blending_state.subtract_alpha)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Use Dual Source?");
      ImGui::TableNextColumn();
      if (data.draw_data.blending_state.use_dual_src)
        ImGui::Text("Yes");
      else
        ImGui::Text("No");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Destination factor");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.draw_data.blending_state.dst_factor).c_str());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Destination alpha factor");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.draw_data.blending_state.dst_factor_alpha).c_str());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Source factor");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.draw_data.blending_state.src_factor).c_str());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Source factor");
      ImGui::TableNextColumn();
      ImGui::Text("%s", fmt::to_string(data.draw_data.blending_state.src_factor_alpha).c_str());

      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("DrawTexturesForm", 2))
    {
      std::map<std::string_view, std::size_t> hash_to_index;
      for (std::size_t i = 0; i < data.draw_data.textures.size(); i++)
      {
        auto& texture = data.draw_data.textures[i];
        hash_to_index[texture.hash_name] = i;
      }

      for (const auto& [hash_name, index] : hash_to_index)
      {
        auto& texture = data.draw_data.textures[index];
        const auto& texture_info =
            m_state.m_runtime_data.m_texture_cache_id_to_data[texture.hash_name];
        const auto& texture_view = texture_info.texture;
        if (texture_view.texture_data)
        {
          auto& sampler = data.draw_data.samplers[texture.unit];

          ImGui::TableNextRow();
          ImGui::TableNextColumn();

          ImGui::Text("Sampler (%i)", texture.unit);

          ImGui::TableNextColumn();

          ImGuiTableFlags flags = ImGuiTableFlags_Borders;
          if (ImGui::BeginTable("WrapModeTable", 2, flags))
          {
            ImGui::TableSetupColumn("Direction");
            ImGui::TableSetupColumn("Wrap Mode");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("u");
            ImGui::TableNextColumn();
            ImGui::Text("%s", fmt::to_string(sampler.tm0.wrap_u).c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("v");
            ImGui::TableNextColumn();
            ImGui::Text("%s", fmt::to_string(sampler.tm0.wrap_v).c_str());
            ImGui::EndTable();
          }

          if (ImGui::BeginTable("FilterModeTable", 2, flags))
          {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Filter Mode");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("min");
            ImGui::TableNextColumn();
            ImGui::Text("%s", fmt::to_string(sampler.tm0.min_filter).c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("mag");
            ImGui::TableNextColumn();
            ImGui::Text("%s", fmt::to_string(sampler.tm0.mag_filter).c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("mip");
            ImGui::TableNextColumn();
            ImGui::Text("%s", fmt::to_string(sampler.tm0.mipmap_filter).c_str());
            ImGui::EndTable();
          }

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Texture (%i)", texture.unit);
          ImGui::TableNextColumn();

          if (texture_info.active)
          {
            const float column_width = ImGui::GetContentRegionAvail().x;
            float image_width = texture_view.texture_data->GetWidth();
            float image_height = texture_view.texture_data->GetHeight();
            const float image_aspect_ratio = image_width / image_height;

            const float final_width = std::min(image_width * 4, column_width);
            image_width = final_width;
            image_height = final_width / image_aspect_ratio;

            ImGui::ImageButton(texture_view.hash_name.data(), *texture_view.texture_data,
                               ImVec2{image_width, image_height});
            if (ImGui::BeginPopupContextItem())
            {
              if (ImGui::Selectable("Dump"))
              {
                VideoCommon::TextureUtils::DumpTexture(*texture_view.texture_data,
                                                       texture.hash_name, 0, false);
              }
              if (ImGui::Selectable("Copy hash"))
              {
                ImGui::SetClipboardText(texture_view.hash_name.data());
              }
              ImGui::EndPopup();
            }
            ImGui::Text("%s %dx%d", texture.hash_name.data(), texture_view.texture_data->GetWidth(),
                        texture_view.texture_data->GetHeight());
          }
          else
          {
            ImGui::Text(
                "<Texture %s unloaded, last created/updated: %lld/%lld>", texture.hash_name.data(),
                static_cast<long long int>(texture_info.create_time.time_since_epoch().count()),
                static_cast<long long int>(texture_info.last_load_time.time_since_epoch().count()));
          }
        }
      }
      ImGui::EndTable();
    }
  }
}

void PropertiesPanel::TextureCacheIDSelected(
    const GraphicsModSystem::TextureCacheID& selected_object)
{
  const auto& data = m_state.m_runtime_data.m_texture_cache_id_to_data[selected_object];
  auto& user_data = m_state.m_user_data.m_texture_cache_id_to_user_data[selected_object];

  if (ImGui::BeginTable("TextureCacheTargetForm", 2))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("DisplayName");
    ImGui::TableNextColumn();
    ImGui::InputText("##TextureCacheTargetDisplayName", &user_data.m_friendly_name);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ID");
    ImGui::TableNextColumn();
    ImGui::Text("%s", data.id.c_str());

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Time Created");
    ImGui::TableNextColumn();
    ImGui::Text("%lld", static_cast<long long int>(data.create_time.time_since_epoch().count()));

    if (data.texture.texture_data && data.active)
    {
      const float column_width = ImGui::GetContentRegionAvail().x;
      float image_width = data.texture.texture_data->GetWidth();
      float image_height = data.texture.texture_data->GetHeight();
      const float image_aspect_ratio = image_width / image_height;

      const float final_width = std::min(image_width * 4, column_width);
      image_width = final_width;
      image_height = final_width / image_aspect_ratio;

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Texture Preview");
      ImGui::TableNextColumn();

      ImGui::ImageButton(data.texture.hash_name.data(), *data.texture.texture_data,
                         ImVec2{image_width, image_height});
      if (ImGui::BeginPopupContextItem())
      {
        if (ImGui::Selectable("Dump"))
        {
          VideoCommon::TextureUtils::DumpTexture(*data.texture.texture_data,
                                                 std::string{data.texture.hash_name}, 0, false);
        }
        if (ImGui::Selectable("Copy hash"))
        {
          ImGui::SetClipboardText(data.texture.hash_name.data());
        }
        ImGui::EndPopup();
      }
      ImGui::Text("%s %dx%d", data.texture.hash_name.data(), data.texture.texture_data->GetWidth(),
                  data.texture.texture_data->GetHeight());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Mip Count");
      ImGui::TableNextColumn();
      ImGui::Text("%d", data.texture.texture_data->GetLevels());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Layer Count");
      ImGui::TableNextColumn();
      ImGui::Text("%d", data.texture.texture_data->GetLayers());
    }

    ImGui::EndTable();
  }

  ShowTags<GraphicsModSystem::TextureTag>(GraphicsModSystem::Runtime::GetTextureTags(),
                                          GraphicsModSystem::Runtime::GetTextureTagConfig(),
                                          &user_data.tag_flags, &m_tag_selection_window_active);
}

void PropertiesPanel::LightSelected(const GraphicsModSystem::LightID& selected_object)
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
    ImGui::TextWrapped("%llu", std::to_underlying(selected_object));

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Time Created");
    ImGui::TableNextColumn();
    ImGui::Text("%lld", static_cast<long long int>(data.m_create_time.time_since_epoch().count()));

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
      overloaded{
          [&](const std::unique_ptr<VideoCommon::MaterialData>& material_data) {
            m_material_control.DrawImGui(selected_object->m_asset_id, material_data.get(),
                                         &selected_object->m_valid);
          },
          [&](const std::unique_ptr<VideoCommon::RasterSurfaceShaderData>& shader_data) {
            m_shader_control.DrawImGui(selected_object->m_asset_id, shader_data.get());
          },
          [&](const std::unique_ptr<VideoCommon::TextureAndSamplerData>& texture_data) {
            auto asset_preview =
                m_state.m_user_data.m_asset_library->GetAssetPreview(selected_object->m_asset_id);
            m_texture_control.DrawImGui(selected_object->m_asset_id, texture_data.get(),
                                        selected_object->m_asset_path,
                                        &selected_object->m_last_data_write, asset_preview);
          },
          [&](const std::unique_ptr<VideoCommon::MeshData>& mesh_data) {
            auto asset_preview =
                m_state.m_user_data.m_asset_library->GetAssetPreview(selected_object->m_asset_id);
            m_mesh_control.DrawImGui(selected_object->m_asset_id, mesh_data.get(),
                                     selected_object->m_asset_path,
                                     &selected_object->m_last_data_write, asset_preview);
          },
          [&](const std::unique_ptr<VideoCommon::RenderTargetData>& render_target_data) {
            auto asset_preview =
                m_state.m_user_data.m_asset_library->GetAssetPreview(selected_object->m_asset_id);
            m_render_target_control.DrawImGui(selected_object->m_asset_id, render_target_data.get(),
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
