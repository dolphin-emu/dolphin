// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/Controls/VertexGroupExtractWindow.h"

#include <string_view>

#include <imgui.h>

namespace GraphicsModEditor::Controls
{
bool ShowVertexGroupExtractWindow(VertexGroupDumper& vertex_group_dumper,
                                  const VertexGroupRecordingRequest& request)
{
  bool result = false;

  const std::string_view vertex_group_extract_popup = "Vertex Group Extract";
  if (!ImGui::IsPopupOpen(vertex_group_extract_popup.data()))
  {
    ImGui::OpenPopup(vertex_group_extract_popup.data());
  }

  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(vertex_group_extract_popup.data(), nullptr))
  {
    ImGui::Text(
        "Vertex group extraction can be used for meshes that perform CPU skinning, to allow "
        "replacement at runtime.");
    ImGui::Text(
        "Directions: start recording, continue to record until you believe the mesh has gone "
        "through enough animations, where each element has been accounted for.");
    ImGui::Text(
        "For example, a jumping animation on a character might target legs and arm movement "
        "but wouldn't account for finger movement.  A second animation would need to be "
        "triggered to target that movement.");

    if (!vertex_group_dumper.IsRecording())
    {
      if (ImGui::Button("Start", ImVec2(120, 0)))
      {
        vertex_group_dumper.Record(request);
      }
    }
    else
    {
      if (ImGui::Button("Stop", ImVec2(120, 0)))
      {
        vertex_group_dumper.StopRecord();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120, 0)))
    {
      // TODO: notify that the recording will be stopped
      if (vertex_group_dumper.IsRecording())
        vertex_group_dumper.StopRecord();
      ImGui::CloseCurrentPopup();
      result = true;
    }
    ImGui::EndPopup();
  }
  return result;
}
}  // namespace GraphicsModEditor::Controls
