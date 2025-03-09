// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/RelativeCameraAction.h"

#include <array>
#include <memory>
#include <ranges>
#include <string_view>

#include <fmt/format.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/JsonUtil.h"
#include "Core/System.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FreeLookCamera.h"
#include "VideoCommon/GraphicsModEditor/EditorBackend.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/EditorMain.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"

std::unique_ptr<RelativeCameraAction>
RelativeCameraAction::Create(const picojson::value& json_data,
                             std::shared_ptr<VideoCommon::CustomAssetLibrary>)
{
  if (!json_data.is<picojson::object>())
    return nullptr;

  // const auto& obj = json_data.get<picojson::object>();

  return std::make_unique<RelativeCameraAction>();
}

RelativeCameraAction::RelativeCameraAction(GraphicsModSystem::Camera camera)
    : m_camera(std::move(camera))
{
}

void RelativeCameraAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->camera) [[unlikely]]
    return;

  *draw_started->camera = m_camera;
}

void RelativeCameraAction::DrawImGui()
{
  if (ImGui::CollapsingHeader("Camera Data", ImGuiTreeNodeFlags_DefaultOpen))
  {
    if (ImGui::BeginTable("Camera Form", 2))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Transform");
      ImGui::TableNextColumn();
      for (std::size_t i = 0; i < 4; i++)
      {
        const std::span<float> vec4 = m_camera.transform.data | std::views::take(4);
        ImGui::InputFloat4(fmt::format("##CameraTransform{}", i).c_str(), vec4.data());
      }
      if (ImGui::Button("Set to current Freelook view"))
      {
        m_camera.transform = g_freelook_camera.GetView();
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Generate new render targets");
      ImGui::TableNextColumn();
      ImGui::Checkbox("##GenerateRenderTargets", &m_camera.generates_render_targets);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Resolution Multiplier");
      ImGui::TableNextColumn();
      if (!m_camera.generates_render_targets)
        ImGui::BeginDisabled();
      if (ImGui::BeginCombo("##ResolutionMultiplier",
                            fmt::to_string(m_camera.resolution_multiplier).c_str()))
      {
        using ResolutionMultiplierValue = std::pair<std::string_view, float>;
        static constexpr std::array<ResolutionMultiplierValue, 4> available = {
            ResolutionMultiplierValue{"0.25", 0.25f}, ResolutionMultiplierValue{"0.5", 0.5f},
            ResolutionMultiplierValue{"1.0", 1.0f}, ResolutionMultiplierValue{"2.0", 2.0f}};
        for (const auto& option : available)
        {
          const bool is_selected = option.second == m_camera.resolution_multiplier;
          if (ImGui::Selectable(fmt::format("{}##", option.first).c_str(), is_selected))
          {
            m_camera.resolution_multiplier = option.second;
          }
        }
        ImGui::EndCombo();
      }
      if (!m_camera.generates_render_targets)
        ImGui::EndDisabled();
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Render Target Type");
      ImGui::TableNextColumn();
      if (!m_camera.generates_render_targets)
        ImGui::BeginDisabled();
      if (ImGui::BeginCombo("##RenderTargetType",
                            fmt::to_string(m_camera.render_target_type).c_str()))
      {
        static constexpr std::array<AbstractTextureType, 3> all_texture_types = {
            AbstractTextureType::Texture_2D, AbstractTextureType::Texture_CubeMap};

        for (const auto& texture_type : all_texture_types)
        {
          const bool is_selected = texture_type == m_camera.render_target_type;
          const auto texture_type_str = fmt::to_string(texture_type);
          if (ImGui::Selectable(fmt::format("{}##", texture_type_str).c_str(), is_selected))
          {
            m_camera.render_target_type = texture_type;
          }
        }
        ImGui::EndCombo();
      }
      if (!m_camera.generates_render_targets)
        ImGui::EndDisabled();
      ImGui::EndTable();
    }
  }

  if (m_camera.generates_render_targets &&
      ImGui::CollapsingHeader("Render Targets", ImGuiTreeNodeFlags_DefaultOpen))
  {
    auto& manager = Core::System::GetInstance().GetGraphicsModManager();
    auto& backend = static_cast<GraphicsModEditor::EditorBackend&>(manager.GetBackend());
    const auto render_targets = backend.GetCameraManager().GetRenderTargets(GetDrawCall());
    for (const auto& [name, render_target_texture] : render_targets)
    {
      // TODO: this is duplicated from property panel (minus copy hash), we should move this to a
      // function
      const float column_width = ImGui::GetContentRegionAvail().x;
      float image_width = render_target_texture->GetWidth();
      float image_height = render_target_texture->GetHeight();
      const float image_aspect_ratio = image_width / image_height;

      const float final_width = std::min(image_width * 4, column_width);
      image_width = final_width;
      image_height = final_width / image_aspect_ratio;

      ImGui::ImageButton(name.data(), *render_target_texture, ImVec2{image_width, image_height});
      if (ImGui::BeginPopupContextItem())
      {
        if (ImGui::Selectable("Dump"))
        {
          VideoCommon::TextureUtils::DumpTexture(*render_target_texture, std::string{name}, 0,
                                                 false);
        }
        ImGui::EndPopup();
      }
      ImGui::Text("%s %dx%d", name.data(), render_target_texture->GetWidth(),
                  render_target_texture->GetHeight());
    }
  }
}

void RelativeCameraAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj["generates_render_targets"] = picojson::value{m_camera.generates_render_targets};
  json_obj["resolution_multiplier"] =
      picojson::value{static_cast<double>(m_camera.resolution_multiplier)};
}

std::string RelativeCameraAction::GetFactoryName() const
{
  return std::string{factory_name};
}
