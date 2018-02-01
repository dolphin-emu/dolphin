// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ---------------------------------------------------------------------------------------------
// GC graphics pipeline
// ---------------------------------------------------------------------------------------------
// 3d commands are issued through the fifo. The GPU draws to the 2MB EFB.
// The efb can be copied back into ram in two forms: as textures or as XFB.
// The XFB is the region in RAM that the VI chip scans out to the television.
// So, after all rendering to EFB is done, the image is copied into one of two XFBs in RAM.
// Next frame, that one is scanned out and the other one gets the copy. = double buffering.
// ---------------------------------------------------------------------------------------------

#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/MathUtil.h"
#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VideoCommon.h"

class PostProcessingShaderImplementation;
enum class EFBAccessType;

struct EfbPokeData
{
  u16 x, y;
  u32 data;
};

// TODO: Move these out of here.
extern int frameCount;
extern int OSDChoice;

// Renderer really isn't a very good name for this class - it's more like "Misc".
// The long term goal is to get rid of this class and replace it with others that make
// more sense.
class Renderer
{
public:
  Renderer(int backbuffer_width, int backbuffer_height);
  virtual ~Renderer();

  enum PixelPerfQuery
  {
    PP_ZCOMP_INPUT_ZCOMPLOC,
    PP_ZCOMP_OUTPUT_ZCOMPLOC,
    PP_ZCOMP_INPUT,
    PP_ZCOMP_OUTPUT,
    PP_BLEND_INPUT,
    PP_EFB_COPY_CLOCKS
  };

  virtual void SetBlendingState(const BlendingState& state) {}
  virtual void SetScissorRect(const EFBRectangle& rc) {}
  virtual void SetRasterizationState(const RasterizationState& state) {}
  virtual void SetDepthState(const DepthState& state) {}
  virtual void SetSamplerState(u32 index, const SamplerState& state) {}
  virtual void SetInterlacingMode() {}
  virtual void SetViewport() {}
  virtual void SetFullscreen(bool enable_fullscreen) {}
  virtual bool IsFullscreen() const { return false; }
  virtual void ApplyState() {}
  virtual void RestoreState() {}
  virtual void ResetAPIState() {}
  virtual void RestoreAPIState() {}
  // Ideal internal resolution - determined by display resolution (automatic scaling) and/or a
  // multiple of the native EFB resolution
  int GetTargetWidth() const { return m_target_width; }
  int GetTargetHeight() const { return m_target_height; }
  // Display resolution
  int GetBackbufferWidth() const { return m_backbuffer_width; }
  int GetBackbufferHeight() const { return m_backbuffer_height; }
  void SetWindowSize(int width, int height);

  // EFB coordinate conversion functions

  // Use this to convert a whole native EFB rect to backbuffer coordinates
  virtual TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) = 0;

  const TargetRectangle& GetTargetRectangle() const { return m_target_rectangle; }
  float CalculateDrawAspectRatio() const;

  std::tuple<float, float> ScaleToDisplayAspectRatio(int width, int height) const;
  TargetRectangle CalculateFrameDumpDrawRectangle() const;
  void UpdateDrawRectangle();

  // Use this to convert a single target rectangle to two stereo rectangles
  std::tuple<TargetRectangle, TargetRectangle>
  ConvertStereoRectangle(const TargetRectangle& rc) const;

  float GetEFBScale() const;

  // Use this to upscale native EFB coordinates to IDEAL internal resolution
  int EFBToScaledX(int x) const;
  int EFBToScaledY(int y) const;

  // Floating point versions of the above - only use them if really necessary
  float EFBToScaledXf(float x) const;
  float EFBToScaledYf(float y) const;

  // Random utilities
  void SaveScreenshot(const std::string& filename, bool wait_for_completion);
  void DrawDebugText();

  virtual void RenderText(const std::string& text, int left, int top, u32 color) = 0;

  virtual void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                           u32 color, u32 z) = 0;
  virtual void SkipClearScreen(bool colorEnable, bool alphaEnable, bool zEnable) = 0;
  virtual void ReinterpretPixelData(unsigned int convtype) = 0;
  void RenderToXFB(u32 xfbAddr, const EFBRectangle& sourceRc, u32 fbStride, u32 fbHeight,
                   float Gamma = 1.0f);

  virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) = 0;
  virtual void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) = 0;

  virtual u16 BBoxRead(int index) = 0;
  virtual void BBoxWrite(int index, u16 value) = 0;

  virtual void AsyncTimewarpDraw() = 0;

  // Finish up the current frame, print some stats
  void Swap(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, u64 ticks,
            float Gamma = 1.0f);
  virtual void SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight,
                        const EFBRectangle& rc, u64 ticks, float Gamma = 1.0f) = 0;

  PEControl::PixelFormat GetPrevPixelFormat() const { return m_prev_efb_format; }
  void StorePixelFormat(PEControl::PixelFormat new_format) { m_prev_efb_format = new_format; }
  PostProcessingShaderImplementation* GetPostProcessor() const { return m_post_processor.get(); }
  // Final surface changing
  // This is called when the surface is resized (WX) or the window changes (Android).
  virtual void ChangeSurface(void* new_surface_handle) {}
  bool UseVertexDepthRange() const;

protected:
  std::tuple<int, int> CalculateTargetScale(int x, int y) const;
  bool CalculateTargetSize();

  bool CheckForHostConfigChanges();

  void CheckFifoRecording();
  void RecordVideoMemory();

  bool IsFrameDumping();
  void DumpFrameData(const u8* data, int w, int h, int stride, const AVIDump::Frame& state,
                     bool swap_upside_down = false);
  void FinishFrameData();

  Common::Flag m_screenshot_request;
  Common::Event m_screenshot_completed;
  std::mutex m_screenshot_lock;
  std::string m_screenshot_name;
public:
  bool m_aspect_wide = false;

protected:
  // The framebuffer size
  int m_target_width = 0;
  int m_target_height = 0;

  // TODO: Add functionality to reinit all the render targets when the window is resized.
  int m_backbuffer_width = 0;
  int m_backbuffer_height = 0;
  int m_last_efb_scale = 0;
  TargetRectangle m_target_rectangle = {};
  bool m_xfb_written = false;

  FPSCounter m_fps_counter;

public:
  std::unique_ptr<PostProcessingShaderImplementation> m_post_processor;

protected:
  static const float GX_MAX_DEPTH;

  Common::Flag m_surface_needs_change;
  Common::Event m_surface_changed;
  void* m_new_surface_handle = nullptr;

  u32 m_last_host_config_bits = 0;

private:
  void RunFrameDumps();
  void ShutdownFrameDumping();

  PEControl::PixelFormat m_prev_efb_format = PEControl::INVALID_FMT;
  unsigned int m_efb_scale_numeratorX = 1;
  unsigned int m_efb_scale_numeratorY = 1;
  unsigned int m_efb_scale_denominatorX = 1;
  unsigned int m_efb_scale_denominatorY = 1;

  // These will be set on the first call to SetWindowSize.
  int m_last_window_request_width = 0;
  int m_last_window_request_height = 0;

  // frame dumping
  std::thread m_frame_dump_thread;
  Common::Event m_frame_dump_start;
  Common::Event m_frame_dump_done;
  Common::Flag m_frame_dump_thread_running;
  u32 m_frame_dump_image_counter = 0;
  bool m_frame_dump_frame_running = false;
  struct FrameDumpConfig
  {
    const u8* data;
    int width;
    int height;
    int stride;
    bool upside_down;
    AVIDump::Frame state;
  } m_frame_dump_config;

  // NOTE: The methods below are called on the framedumping thread.
  bool StartFrameDumpToAVI(const FrameDumpConfig& config);
  void DumpFrameToAVI(const FrameDumpConfig& config);
  void StopFrameDumpToAVI();
  std::string GetFrameDumpNextImageFileName() const;
  bool StartFrameDumpToImage(const FrameDumpConfig& config);
  void DumpFrameToImage(const FrameDumpConfig& config);
};

extern std::unique_ptr<Renderer> g_renderer;
