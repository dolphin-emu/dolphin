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
  Renderer();
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

  virtual void SetColorMask() {}
  virtual void SetBlendMode(bool forceUpdate) {}
  virtual void SetScissorRect(const EFBRectangle& rc) {}
  virtual void SetGenerationMode() {}
  virtual void SetDepthMode() {}
  virtual void SetLogicOpMode() {}
  virtual void SetDitherMode() {}
  virtual void SetSamplerState(int stage, int texindex, bool custom_tex) {}
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
  int GetTargetWidth() { return s_target_width; }
  int GetTargetHeight() { return s_target_height; }
  // Display resolution
  int GetBackbufferWidth() { return s_backbuffer_width; }
  int GetBackbufferHeight() { return s_backbuffer_height; }
  void SetWindowSize(int width, int height);

  // EFB coordinate conversion functions

  // Use this to convert a whole native EFB rect to backbuffer coordinates
  virtual TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) = 0;

  const TargetRectangle& GetTargetRectangle() { return target_rc; }
  float CalculateDrawAspectRatio(int target_width, int target_height);
  std::tuple<float, float> ScaleToDisplayAspectRatio(int width, int height);
  TargetRectangle CalculateFrameDumpDrawRectangle();
  void UpdateDrawRectangle();

  // Use this to convert a single target rectangle to two stereo rectangles
  void ConvertStereoRectangle(const TargetRectangle& rc, TargetRectangle& leftRc,
                              TargetRectangle& rightRc);

  // Use this to upscale native EFB coordinates to IDEAL internal resolution
  int EFBToScaledX(int x);
  int EFBToScaledY(int y);

  // Floating point versions of the above - only use them if really necessary
  float EFBToScaledXf(float x) { return x * ((float)GetTargetWidth() / (float)EFB_WIDTH); }
  float EFBToScaledYf(float y) { return y * ((float)GetTargetHeight() / (float)EFB_HEIGHT); }
  // Random utilities
  void SetScreenshot(const std::string& filename);
  void DrawDebugText();

  virtual void RenderText(const std::string& text, int left, int top, u32 color) = 0;

  virtual void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                           u32 color, u32 z) = 0;
  virtual void ReinterpretPixelData(unsigned int convtype) = 0;
  void RenderToXFB(u32 xfbAddr, const EFBRectangle& sourceRc, u32 fbStride, u32 fbHeight,
                   float Gamma = 1.0f);

  virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) = 0;
  virtual void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) = 0;

  virtual u16 BBoxRead(int index) = 0;
  virtual void BBoxWrite(int index, u16 value) = 0;

  // Finish up the current frame, print some stats
  void Swap(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, u64 ticks,
            float Gamma = 1.0f);
  virtual void SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight,
                        const EFBRectangle& rc, u64 ticks, float Gamma = 1.0f) = 0;

  PEControl::PixelFormat GetPrevPixelFormat() { return prev_efb_format; }
  void StorePixelFormat(PEControl::PixelFormat new_format) { prev_efb_format = new_format; }
  PostProcessingShaderImplementation* GetPostProcessor() { return m_post_processor.get(); }
  // Max height/width
  virtual u32 GetMaxTextureSize() = 0;

  Common::Event s_screenshotCompleted;

  // Final surface changing
  // This is called when the surface is resized (WX) or the window changes (Android).
  virtual void ChangeSurface(void* new_surface_handle) {}
protected:
  void CalculateTargetScale(int x, int y, int* scaledX, int* scaledY);
  bool CalculateTargetSize();

  void CheckFifoRecording();
  void RecordVideoMemory();

  bool IsFrameDumping();
  void DumpFrameData(const u8* data, int w, int h, int stride, const AVIDump::Frame& state,
                     bool swap_upside_down = false);
  void FinishFrameData();

  Common::Flag s_screenshot;
  std::mutex s_criticalScreenshot;
  std::string s_sScreenshotName;

  // The framebuffer size
  int s_target_width = 0;
  int s_target_height = 0;

  // TODO: Add functionality to reinit all the render targets when the window is resized.
  int s_backbuffer_width = 0;
  int s_backbuffer_height = 0;

  TargetRectangle target_rc;

  // TODO: Can probably eliminate this static var.
  int s_last_efb_scale = 0;

  bool XFBWrited = false;

  FPSCounter m_fps_counter;

  std::unique_ptr<PostProcessingShaderImplementation> m_post_processor;

  static const float GX_MAX_DEPTH;

  Common::Flag s_surface_needs_change;
  Common::Event s_surface_changed;
  void* s_new_surface_handle = nullptr;

private:
  void RunFrameDumps();
  void ShutdownFrameDumping();

  PEControl::PixelFormat prev_efb_format = PEControl::INVALID_FMT;
  unsigned int efb_scale_numeratorX = 1;
  unsigned int efb_scale_numeratorY = 1;
  unsigned int efb_scale_denominatorX = 1;
  unsigned int efb_scale_denominatorY = 1;

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
