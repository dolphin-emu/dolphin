// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/CameraChoiceControl.h"

#include <chrono>

#include <imgui.h>

#include "Core/System.h"

#include "VideoCommon/GraphicsModEditor/EditorBackend.h"
#include "VideoCommon/GraphicsModEditor/SceneUtils.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/RelativeCameraAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"

namespace GraphicsModEditor::Controls
{
namespace
{
ImVec2 camera_button_size{150, 150};

std::string GetCameraName(const EditorState& editor_state, GraphicsModSystem::DrawCallID draw_call)
{
  std::string camera_name = "";
  const auto actions = GetActionsForDrawCall<RelativeCameraAction>(editor_state, draw_call);
  if (!actions.empty())
  {
    const std::string draw_call_name = GetDrawCallName(editor_state, draw_call);
    camera_name = fmt::format("{}/{}", draw_call_name, GetActionName(actions[0]));
  }
  return camera_name;
}
}  // namespace
bool CameraChoiceControl(std::string_view popup_name, EditorState* editor_state,
                         GraphicsModSystem::DrawCallID* draw_call_chosen)
{
  if (!editor_state) [[unlikely]]
    return false;
  if (!draw_call_chosen) [[unlikely]]
    return false;

  const std::string camera_name = GetCameraName(*editor_state, *draw_call_chosen);
  if (camera_name.empty())
    ImGui::Text("Camera: None");
  else
    ImGui::Text("Camera: %s", camera_name.c_str());

  if (ImGui::Button("Pick camera"))
  {
    ImGui::OpenPopup(popup_name.data());
  }
  bool changed = false;

  // Camera popup below
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  const ImVec2 size = ImGui::GetMainViewport()->WorkSize;
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(size.x / 4.0f, size.y / 2.0f));
  if (ImGui::BeginPopup(popup_name.data()))
  {
    const u32 column_count = 5;
    u32 current_columns = 0;
    u32 cameras_displayed = 0;

    const float search_size = 200.0f;
    ImGui::SetNextItemWidth(search_size);

    std::string camera_filter = "";
    ImGui::InputTextWithHint("##", "Search...", &camera_filter);

    if (ImGui::BeginTable("CameraPopupTable", column_count))
    {
      ImGui::TableNextRow();

      auto& manager = Core::System::GetInstance().GetGraphicsModManager();
      auto& backend = static_cast<GraphicsModEditor::EditorBackend&>(manager.GetBackend());
      const auto camera_ids = backend.GetCameraManager().GetDrawCallsWithCameras();
      for (const auto& draw_call : camera_ids)
      {
        const std::string camera_name_in_popup = GetCameraName(*editor_state, draw_call);
        if (camera_name_in_popup.empty())
          continue;
        if (!camera_filter.empty() && camera_name_in_popup.find(camera_filter) == std::string::npos)
        {
          continue;
        }

        cameras_displayed++;
        ImGui::TableNextColumn();
        if (ImGui::Button(camera_name_in_popup.c_str()))
        {
          *draw_call_chosen = draw_call;
          changed = true;
          ImGui::CloseCurrentPopup();
        }

        current_columns++;
        if (current_columns == column_count)
        {
          ImGui::TableNextRow();
          current_columns = 0;
        }
      }
      ImGui::EndTable();
    }

    if (cameras_displayed == 0)
    {
      ImGui::Text("No cameras found");
    }
    ImGui::EndPopup();
  }

  return changed;
}
}  // namespace GraphicsModEditor::Controls
