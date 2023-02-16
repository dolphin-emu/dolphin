// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/OnScreenUIKeyMap.h"

class NativeVertexFormat;
class AbstractTexture;
class AbstractPipeline;

namespace VideoCommon
{
// OnScreenUI handles all the ImGui rendering.
class OnScreenUI
{
public:
  OnScreenUI() = default;
  ~OnScreenUI();

  // ImGui initialization depends on being able to create textures and pipelines, so do it last.
  bool Initialize(u32 width, u32 height, float scale);

  // Returns a lock for the ImGui mutex, enabling data structures to be modified from outside.
  // Use with care, only non-drawing functions should be called from outside the video thread,
  // as the drawing is tied to a "frame".
  std::unique_lock<std::mutex> GetImGuiLock();

  bool IsReady() { return m_ready; }

  // Sets up ImGui state for the next frame.
  // This function itself acquires the ImGui lock, so it should not be held.
  void BeginImGuiFrame(u32 width, u32 height);

  // Same as above but without locking the ImGui lock.
  void BeginImGuiFrameUnlocked(u32 width, u32 height);

  // Renders ImGui windows to the currently-bound framebuffer.
  // Should be called with the ImGui lock held.
  void DrawImGui();

  // Recompiles ImGui pipeline - call when stereo mode changes.
  bool RecompileImGuiPipeline();

  void SetScale(float backbuffer_scale);

  void Finalize();

  // Receive keyboard and mouse from QT
  void SetKeyMap(const DolphinKeyMap& key_map);
  void SetKey(u32 key, bool is_down, const char* chars);
  void SetMousePos(float x, float y);
  void SetMousePress(u32 button_mask);

private:
  void DrawDebugText();

  // ImGui resources.
  std::unique_ptr<NativeVertexFormat> m_imgui_vertex_format;
  std::vector<std::unique_ptr<AbstractTexture>> m_imgui_textures;
  std::unique_ptr<AbstractPipeline> m_imgui_pipeline;
  std::mutex m_imgui_mutex;
  u64 m_imgui_last_frame_time = 0;

  u32 m_backbuffer_width = 1;
  u32 m_backbuffer_height = 1;
  float m_backbuffer_scale = 1.0;

  bool m_ready = false;
};
}  // namespace VideoCommon
