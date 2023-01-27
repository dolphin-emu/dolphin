// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Flag.h"
#include "Common/MathUtil.h"

#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConfig.h"

#include <array>
#include <memory>
#include <mutex>
#include <span>
#include <tuple>

class AbstractTexture;

namespace VideoCommon
{
class OnScreenUI;
class PostProcessing;

class Presenter
{
public:
  using ClearColor = std::array<float, 4>;

  Presenter();
  virtual ~Presenter();

  bool SubmitXFB(RcTcacheEntry xfb_entry, MathUtil::Rectangle<int>& xfb_rect, u64 ticks,
                 int frame_count);
  void Present();
  void ClearLastXfbId() { m_last_xfb_id = std::numeric_limits<u64>::max(); }

  bool Initialize();

  void CheckForConfigChanges(u32 changed_bits);

  // Begins/presents a "UI frame". UI frames do not draw any of the console XFB, but this could
  // change in the future.
  void BeginUIFrame();
  void EndUIFrame();

  // Display resolution
  int GetBackbufferWidth() const { return m_backbuffer_width; }
  int GetBackbufferHeight() const { return m_backbuffer_height; }
  float GetBackbufferScale() const { return m_backbuffer_scale; }
  AbstractTextureFormat GetBackbufferFormat() const { return m_backbuffer_format; }
  void SetWindowSize(int width, int height);
  void SetBackbuffer(int backbuffer_width, int backbuffer_height);
  void SetBackbuffer(int backbuffer_width, int backbuffer_height, float backbuffer_scale,
                     AbstractTextureFormat backbuffer_format);

  void UpdateDrawRectangle();

  float CalculateDrawAspectRatio() const;

  // Crops the target rectangle to the framebuffer dimensions, reducing the size of the source
  // rectangle if it is greater. Works even if the source and target rectangles don't have a
  // 1:1 pixel mapping, scaling as appropriate.
  void AdjustRectanglesToFitBounds(MathUtil::Rectangle<int>* target_rect,
                                   MathUtil::Rectangle<int>* source_rect, int fb_width,
                                   int fb_height);

  void ReleaseXFBContentLock();

  // Draws the specified XFB buffer to the screen, performing any post-processing.
  // Assumes that the backbuffer has already been bound and cleared.
  virtual void RenderXFBToScreen(const MathUtil::Rectangle<int>& target_rc,
                                 const AbstractTexture* source_texture,
                                 const MathUtil::Rectangle<int>& source_rc);

  VideoCommon::PostProcessing* GetPostProcessor() const { return m_post_processor.get(); }
  // Final surface changing
  // This is called when the surface is resized (WX) or the window changes (Android).
  void ChangeSurface(void* new_surface_handle);
  void ResizeSurface();
  bool SurfaceResizedTestAndClear() { return m_surface_resized.TestAndClear(); }
  bool SurfaceChangedTestAndClear() { return m_surface_changed.TestAndClear(); }
  void* GetNewSurfaceHandle();

  void SetKeyMap(std::span<std::array<int, 2>> key_map);

  void SetKey(u32 key, bool is_down, const char* chars);
  void SetMousePos(float x, float y);
  void SetMousePress(u32 button_mask);

  const MathUtil::Rectangle<int>& GetTargetRectangle() const { return m_target_rectangle; }

private:
  std::tuple<int, int> CalculateOutputDimensions(int width, int height) const;
  std::tuple<float, float> ApplyStandardAspectCrop(float width, float height) const;
  std::tuple<float, float> ScaleToDisplayAspectRatio(int width, int height) const;

  // Use this to convert a single target rectangle to two stereo rectangles
  std::tuple<MathUtil::Rectangle<int>, MathUtil::Rectangle<int>>
  ConvertStereoRectangle(const MathUtil::Rectangle<int>& rc) const;

  std::mutex m_swap_mutex;

  // Backbuffer (window) size and render area
  int m_backbuffer_width = 0;
  int m_backbuffer_height = 0;
  float m_backbuffer_scale = 1.0f;
  AbstractTextureFormat m_backbuffer_format = AbstractTextureFormat::Undefined;

  void* m_new_surface_handle = nullptr;
  Common::Flag m_surface_changed;
  Common::Flag m_surface_resized;

  MathUtil::Rectangle<int> m_target_rectangle = {};

  RcTcacheEntry m_xfb_entry;
  MathUtil::Rectangle<int> m_xfb_rect;

  // Tracking of XFB textures so we don't render duplicate frames.
  u64 m_last_xfb_id = std::numeric_limits<u64>::max();

  // These will be set on the first call to SetWindowSize.
  int m_last_window_request_width = 0;
  int m_last_window_request_height = 0;

  std::unique_ptr<VideoCommon::PostProcessing> m_post_processor;
  std::unique_ptr<VideoCommon::OnScreenUI> m_onscreen_ui;
};

}  // namespace VideoCommon

extern std::unique_ptr<VideoCommon::Presenter> g_presenter;
