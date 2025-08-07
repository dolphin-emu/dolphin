// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/RelativeCameraAction.h"

#include <array>
#include <memory>
#include <ranges>
#include <string_view>

#include <fmt/format.h>
#include <imgui.h>

#include "Core/System.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FreeLookCamera.h"
#include "VideoCommon/GraphicsModEditor/Controls/AssetDisplay.h"
#include "VideoCommon/GraphicsModEditor/EditorEvents.h"
#include "VideoCommon/GraphicsModEditor/EditorMain.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/Resources/CustomResourceManager.h"

std::unique_ptr<RelativeCameraAction>
RelativeCameraAction::Create(const picojson::value& json_data,
                             std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  if (!json_data.is<picojson::object>())
    return nullptr;

  // const auto& obj = json_data.get<picojson::object>();

  return std::make_unique<RelativeCameraAction>(std::move(library));
}

RelativeCameraAction::RelativeCameraAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
    : m_library(std::move(library))
{
}

RelativeCameraAction::RelativeCameraAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                                           GraphicsModSystem::Camera camera)
    : m_library(std::move(library)), m_camera(std::move(camera))
{
}

void RelativeCameraAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->camera) [[unlikely]]
    return;

  auto& resource_manager = Core::System::GetInstance().GetCustomResourceManager();
  const auto resource = resource_manager.GetRenderTargetFromAsset(m_color_rt_asset_id, m_library);
  if (resource)
  {
    // resource->GetData();
    *draw_started->camera = m_camera;
  }
}

void RelativeCameraAction::DrawImGui()
{
  auto& editor = Core::System::GetInstance().GetGraphicsModEditor();
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
      ImGui::Text("Custom render taget");
      ImGui::TableNextColumn();
      if (GraphicsModEditor::Controls::AssetDisplay("Render target", editor.GetEditorState(),
                                                    &m_color_rt_asset_id,
                                                    GraphicsModEditor::RenderTarget))
      {
        GraphicsModEditor::EditorEvents::AssetReloadEvent::Trigger(m_camera.asset_id);
      }
      ImGui::EndTable();
    }
  }
}

void RelativeCameraAction::SerializeToConfig(picojson::object* obj)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj["color_rt_asset"] = picojson::value{m_color_rt_asset_id};
}

std::string RelativeCameraAction::GetFactoryName() const
{
  return std::string{factory_name};
}
