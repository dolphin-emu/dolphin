// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <mutex>

#include "VideoCommon/NativeVertexFormat.h"

enum class AbstractTextureFormat : u32;
class AbstractTexture;
class AbstractPipeline;

namespace VideoCommon
{
class ImGuiManager
{
public:
  ImGuiManager(AbstractTextureFormat backbuffer_format, float backbuffer_scale);
  ~ImGuiManager();

  // ImGui initialization depends on being able to create textures and pipelines, so do it last.
  bool Initialize();

  // Cleans up all resources.
  void Shutdown();

  // Draws all global imgui windows.
  // Should be called with the ImGui lock held.
  void Draw();

  // Renders ImGui windows to the currently-bound framebuffer.
  // No need to lock, as the draw data should be finalized by Draw().
  void Render();

  // Sets up ImGui state for the next frame. Call after presenting.
  // This function itself acquires the ImGui lock, so it should not be held.
  void EndFrame();

  // Returns a lock for the ImGui mutex, enabling data structures to be modified from outside.
  // Use with care, only non-drawing functions should be called from outside the video thread,
  // as the drawing is tied to a "frame".
  std::unique_lock<std::mutex> GetStateLock();

private:
  // Various window drawing functions.
  void DrawFPSWindow();
  void DrawMovieWindow();

  std::unique_ptr<NativeVertexFormat> m_imgui_vertex_format;
  std::vector<std::unique_ptr<AbstractTexture>> m_imgui_textures;
  std::unique_ptr<AbstractPipeline> m_imgui_pipeline;
  AbstractTextureFormat m_backbuffer_format;
  float m_backbuffer_scale;

  // Mutex protecting global imgui state, use when injecting input events.
  std::mutex m_state_mutex;
  u64 m_last_frame_time;
};
}  // namespace VideoCommon

extern std::unique_ptr<VideoCommon::ImGuiManager> g_imgui_manager;
