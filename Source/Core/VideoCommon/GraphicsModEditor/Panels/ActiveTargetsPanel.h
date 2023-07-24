// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <set>
#include <vector>

#include "Common/HookableEvent.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModEditor::Panels
{
class ActiveTargetsPanel
{
public:
  explicit ActiveTargetsPanel(EditorState& state);

  // Renders ImGui windows to the currently-bound framebuffer.
  void DrawImGui();

  void AddDrawCall(DrawCallData draw_call);
  void AddFBCall(FBCallData fb_call);
  void AddLightData(LightData light_data);

private:
  void DrawCallPanel();
  void EFBPanel();
  void LightPanel();
  void EndOfFrame();
  void SelectionChanged();
  Common::EventHook m_end_of_frame_event;
  Common::EventHook m_selection_event;

  EditorState& m_state;

  // Target tracking
  std::vector<const DrawCallData*> m_current_draw_calls;
  std::vector<const DrawCallData*> m_draw_calls_to_clean;
  std::map<GraphicsMods::DrawCallID, DrawCallData> m_new_draw_call_id_to_data;

  std::vector<FBCallData*> m_current_fb_calls;
  std::vector<FBCallData*> m_upcoming_fb_calls;
  std::map<FBInfo, FBCallData> m_upcoming_fb_call_id_to_data;

  std::vector<const LightData*> m_current_lights;
  std::vector<const LightData*> m_lights_to_clean;
  std::map<GraphicsMods::LightID, LightData> m_new_light_id_to_data;

  // Track open nodes
  std::set<GraphicsMods::DrawCallID> m_open_draw_call_nodes;
  std::set<FBInfo> m_open_fb_call_nodes;
  std::set<GraphicsMods::LightID> m_open_light_nodes;

  // Selected nodes
  std::set<SelectableType> m_selected_nodes;
  bool m_selection_list_changed;

  // Mesh extraction window details
  SceneDumper::RecordingRequest m_last_mesh_dump_request;
  bool m_open_mesh_dump_export_window = false;
};
}  // namespace GraphicsModEditor::Panels
