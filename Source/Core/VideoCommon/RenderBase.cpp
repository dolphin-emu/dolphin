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

#include "VideoCommon/RenderBase.h"

#include <cinttypes>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Profiler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/VideoInterface.h"
#include "Core/Host.h"
#include "Core/Movie.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

// TODO: Move these out of here.
int frameCount;
int OSDChoice;
static int OSDTime;

std::unique_ptr<Renderer> g_renderer;

std::mutex Renderer::s_criticalScreenshot;
std::string Renderer::s_sScreenshotName;

Common::Event Renderer::s_screenshotCompleted;
Common::Flag Renderer::s_screenshot;

// The framebuffer size
int Renderer::s_target_width;
int Renderer::s_target_height;

// TODO: Add functionality to reinit all the render targets when the window is resized.
int Renderer::s_backbuffer_width;
int Renderer::s_backbuffer_height;

std::unique_ptr<PostProcessingShaderImplementation> Renderer::m_post_processor;

// Final surface changing
Common::Flag Renderer::s_surface_needs_change;
Common::Event Renderer::s_surface_changed;
void* Renderer::s_new_surface_handle;

TargetRectangle Renderer::target_rc;

int Renderer::s_last_efb_scale;

bool Renderer::XFBWrited;

PEControl::PixelFormat Renderer::prev_efb_format = PEControl::INVALID_FMT;
unsigned int Renderer::efb_scale_numeratorX = 1;
unsigned int Renderer::efb_scale_numeratorY = 1;
unsigned int Renderer::efb_scale_denominatorX = 1;
unsigned int Renderer::efb_scale_denominatorY = 1;

// The maximum depth that is written to the depth buffer should never exceed this value.
// This is necessary because we use a 2^24 divisor for all our depth values to prevent
// floating-point round-trip errors. However the console GPU doesn't ever write a value
// to the depth buffer that exceeds 2^24 - 1.
const float Renderer::GX_MAX_DEPTH = 16777215.0f / 16777216.0f;

static float AspectToWidescreen(float aspect)
{
  return aspect * ((16.0f / 9.0f) / (4.0f / 3.0f));
}

Renderer::Renderer()
{
  UpdateActiveConfig();

  OSDChoice = 0;
  OSDTime = 0;
}

Renderer::~Renderer()
{
  // invalidate previous efb format
  prev_efb_format = PEControl::INVALID_FMT;

  efb_scale_numeratorX = efb_scale_numeratorY = efb_scale_denominatorX = efb_scale_denominatorY = 1;

  ShutdownFrameDumping();
  if (m_frame_dump_thread.joinable())
    m_frame_dump_thread.join();
}

void Renderer::RenderToXFB(u32 xfbAddr, const EFBRectangle& sourceRc, u32 fbStride, u32 fbHeight,
                           float Gamma)
{
  CheckFifoRecording();

  if (!fbStride || !fbHeight)
    return;

  XFBWrited = true;

  if (g_ActiveConfig.bUseXFB)
  {
    FramebufferManagerBase::CopyToXFB(xfbAddr, fbStride, fbHeight, sourceRc, Gamma);
  }
  else
  {
    // The timing is not predictable here. So try to use the XFB path to dump frames.
    u64 ticks = CoreTiming::GetTicks();

    // below div two to convert from bytes to pixels - it expects width, not stride
    Swap(xfbAddr, fbStride / 2, fbStride / 2, fbHeight, sourceRc, ticks, Gamma);
  }
}

int Renderer::EFBToScaledX(int x)
{
  switch (g_ActiveConfig.iEFBScale)
  {
  case SCALE_AUTO:  // fractional
    return FramebufferManagerBase::ScaleToVirtualXfbWidth(x);

  default:
    return x * (int)efb_scale_numeratorX / (int)efb_scale_denominatorX;
  };
}

int Renderer::EFBToScaledY(int y)
{
  switch (g_ActiveConfig.iEFBScale)
  {
  case SCALE_AUTO:  // fractional
    return FramebufferManagerBase::ScaleToVirtualXfbHeight(y);

  default:
    return y * (int)efb_scale_numeratorY / (int)efb_scale_denominatorY;
  };
}

void Renderer::CalculateTargetScale(int x, int y, int* scaledX, int* scaledY)
{
  if (g_ActiveConfig.iEFBScale == SCALE_AUTO || g_ActiveConfig.iEFBScale == SCALE_AUTO_INTEGRAL)
  {
    *scaledX = x;
    *scaledY = y;
  }
  else
  {
    *scaledX = x * (int)efb_scale_numeratorX / (int)efb_scale_denominatorX;
    *scaledY = y * (int)efb_scale_numeratorY / (int)efb_scale_denominatorY;
  }
}

// return true if target size changed
bool Renderer::CalculateTargetSize()
{
  int newEFBWidth, newEFBHeight;
  newEFBWidth = newEFBHeight = 0;

  // TODO: Ugly. Clean up
  switch (s_last_efb_scale)
  {
  case SCALE_AUTO:
  case SCALE_AUTO_INTEGRAL:
    newEFBWidth = FramebufferManagerBase::ScaleToVirtualXfbWidth(EFB_WIDTH);
    newEFBHeight = FramebufferManagerBase::ScaleToVirtualXfbHeight(EFB_HEIGHT);

    if (s_last_efb_scale == SCALE_AUTO_INTEGRAL)
    {
      efb_scale_numeratorX = efb_scale_numeratorY =
          std::max((newEFBWidth - 1) / EFB_WIDTH + 1, (newEFBHeight - 1) / EFB_HEIGHT + 1);
      efb_scale_denominatorX = efb_scale_denominatorY = 1;
      newEFBWidth = EFBToScaledX(EFB_WIDTH);
      newEFBHeight = EFBToScaledY(EFB_HEIGHT);
    }
    else
    {
      efb_scale_numeratorX = newEFBWidth;
      efb_scale_denominatorX = EFB_WIDTH;
      efb_scale_numeratorY = newEFBHeight;
      efb_scale_denominatorY = EFB_HEIGHT;
    }
    break;

  case SCALE_1X:
    efb_scale_numeratorX = efb_scale_numeratorY = 1;
    efb_scale_denominatorX = efb_scale_denominatorY = 1;
    break;

  case SCALE_1_5X:
    efb_scale_numeratorX = efb_scale_numeratorY = 3;
    efb_scale_denominatorX = efb_scale_denominatorY = 2;
    break;

  case SCALE_2X:
    efb_scale_numeratorX = efb_scale_numeratorY = 2;
    efb_scale_denominatorX = efb_scale_denominatorY = 1;
    break;

  case SCALE_2_5X:
    efb_scale_numeratorX = efb_scale_numeratorY = 5;
    efb_scale_denominatorX = efb_scale_denominatorY = 2;
    break;

  default:
    efb_scale_numeratorX = efb_scale_numeratorY = s_last_efb_scale - 3;
    efb_scale_denominatorX = efb_scale_denominatorY = 1;

    const u32 max_size = GetMaxTextureSize();
    if (max_size < EFB_WIDTH * efb_scale_numeratorX / efb_scale_denominatorX)
    {
      efb_scale_numeratorX = efb_scale_numeratorY = (max_size / EFB_WIDTH);
      efb_scale_denominatorX = efb_scale_denominatorY = 1;
    }

    break;
  }
  if (s_last_efb_scale > SCALE_AUTO_INTEGRAL)
    CalculateTargetScale(EFB_WIDTH, EFB_HEIGHT, &newEFBWidth, &newEFBHeight);

  if (newEFBWidth != s_target_width || newEFBHeight != s_target_height)
  {
    s_target_width = newEFBWidth;
    s_target_height = newEFBHeight;
    return true;
  }
  return false;
}

void Renderer::ConvertStereoRectangle(const TargetRectangle& rc, TargetRectangle& leftRc,
                                      TargetRectangle& rightRc)
{
  // Resize target to half its original size
  TargetRectangle drawRc = rc;
  if (g_ActiveConfig.iStereoMode == STEREO_TAB)
  {
    // The height may be negative due to flipped rectangles
    int height = rc.bottom - rc.top;
    drawRc.top += height / 4;
    drawRc.bottom -= height / 4;
  }
  else
  {
    int width = rc.right - rc.left;
    drawRc.left += width / 4;
    drawRc.right -= width / 4;
  }

  // Create two target rectangle offset to the sides of the backbuffer
  leftRc = drawRc, rightRc = drawRc;
  if (g_ActiveConfig.iStereoMode == STEREO_TAB)
  {
    leftRc.top -= s_backbuffer_height / 4;
    leftRc.bottom -= s_backbuffer_height / 4;
    rightRc.top += s_backbuffer_height / 4;
    rightRc.bottom += s_backbuffer_height / 4;
  }
  else
  {
    leftRc.left -= s_backbuffer_width / 4;
    leftRc.right -= s_backbuffer_width / 4;
    rightRc.left += s_backbuffer_width / 4;
    rightRc.right += s_backbuffer_width / 4;
  }
}

void Renderer::SetScreenshot(const std::string& filename)
{
  std::lock_guard<std::mutex> lk(s_criticalScreenshot);
  s_sScreenshotName = filename;
  s_screenshot.Set();
}

// Create On-Screen-Messages
void Renderer::DrawDebugText()
{
  std::string final_yellow, final_cyan;

  if (g_ActiveConfig.bShowFPS || SConfig::GetInstance().m_ShowFrameCount)
  {
    if (g_ActiveConfig.bShowFPS)
      final_cyan += StringFromFormat("FPS: %u", g_renderer->m_fps_counter.GetFPS());

    if (g_ActiveConfig.bShowFPS && SConfig::GetInstance().m_ShowFrameCount)
      final_cyan += " - ";
    if (SConfig::GetInstance().m_ShowFrameCount)
    {
      final_cyan += StringFromFormat("Frame: %" PRIu64, Movie::GetCurrentFrame());
      if (Movie::IsPlayingInput())
        final_cyan += StringFromFormat("\nInput: %" PRIu64 " / %" PRIu64,
                                       Movie::GetCurrentInputCount(), Movie::GetTotalInputCount());
    }

    final_cyan += "\n";
    final_yellow += "\n";
  }

  if (SConfig::GetInstance().m_ShowLag)
  {
    final_cyan += StringFromFormat("Lag: %" PRIu64 "\n", Movie::GetCurrentLagCount());
    final_yellow += "\n";
  }

  if (SConfig::GetInstance().m_ShowInputDisplay)
  {
    final_cyan += Movie::GetInputDisplay();
    final_yellow += "\n";
  }

  if (SConfig::GetInstance().m_ShowRTC)
  {
    final_cyan += Movie::GetRTCDisplay();
    final_yellow += "\n";
  }

  // OSD Menu messages
  if (OSDChoice > 0)
  {
    OSDTime = Common::Timer::GetTimeMs() + 3000;
    OSDChoice = -OSDChoice;
  }

  if ((u32)OSDTime > Common::Timer::GetTimeMs())
  {
    std::string res_text;
    switch (g_ActiveConfig.iEFBScale)
    {
    case SCALE_AUTO:
      res_text = "Auto (fractional)";
      break;
    case SCALE_AUTO_INTEGRAL:
      res_text = "Auto (integral)";
      break;
    case SCALE_1X:
      res_text = "Native";
      break;
    case SCALE_1_5X:
      res_text = "1.5x";
      break;
    case SCALE_2X:
      res_text = "2x";
      break;
    case SCALE_2_5X:
      res_text = "2.5x";
      break;
    default:
      res_text = StringFromFormat("%dx", g_ActiveConfig.iEFBScale - 3);
      break;
    }
    const char* ar_text = "";
    switch (g_ActiveConfig.iAspectRatio)
    {
    case ASPECT_AUTO:
      ar_text = "Auto";
      break;
    case ASPECT_STRETCH:
      ar_text = "Stretch";
      break;
    case ASPECT_ANALOG:
      ar_text = "Force 4:3";
      break;
    case ASPECT_ANALOG_WIDE:
      ar_text = "Force 16:9";
    }

    const char* const efbcopy_text = g_ActiveConfig.bSkipEFBCopyToRam ? "to Texture" : "to RAM";

    // The rows
    const std::string lines[] = {
        std::string("Internal Resolution: ") + res_text,
        std::string("Aspect Ratio: ") + ar_text + (g_ActiveConfig.bCrop ? " (crop)" : ""),
        std::string("Copy EFB: ") + efbcopy_text,
        std::string("Fog: ") + (g_ActiveConfig.bDisableFog ? "Disabled" : "Enabled"),
        SConfig::GetInstance().m_EmulationSpeed <= 0 ?
            "Speed Limit: Unlimited" :
            StringFromFormat("Speed Limit: %li%%",
                             std::lround(SConfig::GetInstance().m_EmulationSpeed * 100.f)),
    };

    enum
    {
      lines_count = sizeof(lines) / sizeof(*lines)
    };

    // The latest changed setting in yellow
    for (int i = 0; i != lines_count; ++i)
    {
      if (OSDChoice == -i - 1)
        final_yellow += lines[i];
      final_yellow += '\n';
    }

    // The other settings in cyan
    for (int i = 0; i != lines_count; ++i)
    {
      if (OSDChoice != -i - 1)
        final_cyan += lines[i];
      final_cyan += '\n';
    }
  }

  final_cyan += Common::Profiler::ToString();

  if (g_ActiveConfig.bOverlayStats)
    final_cyan += Statistics::ToString();

  if (g_ActiveConfig.bOverlayProjStats)
    final_cyan += Statistics::ToStringProj();

  // and then the text
  g_renderer->RenderText(final_cyan, 20, 20, 0xFF00FFFF);
  g_renderer->RenderText(final_yellow, 20, 20, 0xFFFFFF00);
}

float Renderer::CalculateDrawAspectRatio(int target_width, int target_height)
{
  // The dimensions are the sizes that are used to create the EFB/backbuffer textures, so
  // they should always be greater than zero.
  _assert_(target_width > 0 && target_height > 0);
  if (g_ActiveConfig.iAspectRatio == ASPECT_STRETCH)
  {
    // If stretch is enabled, we prefer the aspect ratio of the window.
    return (static_cast<float>(target_width) / static_cast<float>(target_height)) /
           (static_cast<float>(s_backbuffer_width) / static_cast<float>(s_backbuffer_height));
  }

  // The rendering window aspect ratio as a proportion of the 4:3 or 16:9 ratio
  if (g_ActiveConfig.iAspectRatio == ASPECT_ANALOG_WIDE ||
      (g_ActiveConfig.iAspectRatio != ASPECT_ANALOG && Core::g_aspect_wide))
  {
    return (static_cast<float>(target_width) / static_cast<float>(target_height)) /
           AspectToWidescreen(VideoInterface::GetAspectRatio());
  }
  else
  {
    return (static_cast<float>(target_width) / static_cast<float>(target_height)) /
           VideoInterface::GetAspectRatio();
  }
}

std::tuple<float, float> Renderer::ScaleToDisplayAspectRatio(const int width, const int height)
{
  // Scale either the width or height depending the content aspect ratio.
  // This way we preserve as much resolution as possible when scaling.
  float ratio = CalculateDrawAspectRatio(width, height);
  if (ratio >= 1.0f)
  {
    // Preserve horizontal resolution, scale vertically.
    return std::make_tuple(static_cast<float>(width), static_cast<float>(height) * ratio);
  }

  // Preserve vertical resolution, scale horizontally.
  return std::make_tuple(static_cast<float>(width) / ratio, static_cast<float>(height));
}

TargetRectangle Renderer::CalculateFrameDumpDrawRectangle()
{
  // No point including any borders in the frame dump image, since they'd have to be cropped anyway.
  TargetRectangle rc;
  rc.left = 0;
  rc.top = 0;

  // If full-resolution frame dumping is disabled, just use the window draw rectangle.
  // Also do this if RealXFB is enabled, since the image has been downscaled for the XFB copy
  // anyway, and there's no point writing an upscaled frame with no filtering.
  if (!g_ActiveConfig.bInternalResolutionFrameDumps || g_ActiveConfig.RealXFBEnabled())
  {
    // But still remove the borders, since the caller expects this.
    rc.right = target_rc.GetWidth();
    rc.bottom = target_rc.GetHeight();
    return rc;
  }

  // Grab the dimensions of the EFB textures, we scale either of these depending on the ratio.
  u32 efb_width, efb_height;
  std::tie(efb_width, efb_height) = g_framebuffer_manager->GetTargetSize();

  float draw_width, draw_height;
  std::tie(draw_width, draw_height) = ScaleToDisplayAspectRatio(efb_width, efb_height);

  rc.right = static_cast<int>(std::ceil(draw_width));
  rc.bottom = static_cast<int>(std::ceil(draw_height));
  return rc;
}

void Renderer::UpdateDrawRectangle()
{
  float FloatGLWidth = static_cast<float>(s_backbuffer_width);
  float FloatGLHeight = static_cast<float>(s_backbuffer_height);
  float FloatXOffset = 0;
  float FloatYOffset = 0;

  // The rendering window size
  const float WinWidth = FloatGLWidth;
  const float WinHeight = FloatGLHeight;

  // Update aspect ratio hack values
  // Won't take effect until next frame
  // Don't know if there is a better place for this code so there isn't a 1 frame delay
  if (g_ActiveConfig.bWidescreenHack)
  {
    float source_aspect = VideoInterface::GetAspectRatio();
    if (Core::g_aspect_wide)
      source_aspect = AspectToWidescreen(source_aspect);
    float target_aspect;

    switch (g_ActiveConfig.iAspectRatio)
    {
    case ASPECT_STRETCH:
      target_aspect = WinWidth / WinHeight;
      break;
    case ASPECT_ANALOG:
      target_aspect = VideoInterface::GetAspectRatio();
      break;
    case ASPECT_ANALOG_WIDE:
      target_aspect = AspectToWidescreen(VideoInterface::GetAspectRatio());
      break;
    default:
      // ASPECT_AUTO
      target_aspect = source_aspect;
      break;
    }

    float adjust = source_aspect / target_aspect;
    if (adjust > 1)
    {
      // Vert+
      g_Config.fAspectRatioHackW = 1;
      g_Config.fAspectRatioHackH = 1 / adjust;
    }
    else
    {
      // Hor+
      g_Config.fAspectRatioHackW = adjust;
      g_Config.fAspectRatioHackH = 1;
    }
  }
  else
  {
    // Hack is disabled
    g_Config.fAspectRatioHackW = 1;
    g_Config.fAspectRatioHackH = 1;
  }

  // Check for force-settings and override.

  // The rendering window aspect ratio as a proportion of the 4:3 or 16:9 ratio
  float Ratio = CalculateDrawAspectRatio(s_backbuffer_width, s_backbuffer_height);
  if (g_ActiveConfig.iAspectRatio != ASPECT_STRETCH)
  {
    if (Ratio >= 0.995f && Ratio <= 1.005f)
    {
      // If we're very close already, don't scale.
      Ratio = 1.0f;
    }
    else if (Ratio > 1.0f)
    {
      // Scale down and center in the X direction.
      FloatGLWidth /= Ratio;
      FloatXOffset = (WinWidth - FloatGLWidth) / 2.0f;
    }
    // The window is too high, we have to limit the height
    else
    {
      // Scale down and center in the Y direction.
      FloatGLHeight *= Ratio;
      FloatYOffset = FloatYOffset + (WinHeight - FloatGLHeight) / 2.0f;
    }
  }

  // -----------------------------------------------------------------------
  // Crop the picture from Analog to 4:3 or from Analog (Wide) to 16:9.
  // Output: FloatGLWidth, FloatGLHeight, FloatXOffset, FloatYOffset
  // ------------------
  if (g_ActiveConfig.iAspectRatio != ASPECT_STRETCH && g_ActiveConfig.bCrop)
  {
    Ratio = (4.0f / 3.0f) / VideoInterface::GetAspectRatio();
    if (Ratio <= 1.0f)
    {
      Ratio = 1.0f / Ratio;
    }
    // The width and height we will add (calculate this before FloatGLWidth and FloatGLHeight is
    // adjusted)
    float IncreasedWidth = (Ratio - 1.0f) * FloatGLWidth;
    float IncreasedHeight = (Ratio - 1.0f) * FloatGLHeight;
    // The new width and height
    FloatGLWidth = FloatGLWidth * Ratio;
    FloatGLHeight = FloatGLHeight * Ratio;
    // Adjust the X and Y offset
    FloatXOffset = FloatXOffset - (IncreasedWidth * 0.5f);
    FloatYOffset = FloatYOffset - (IncreasedHeight * 0.5f);
  }

  int XOffset = (int)(FloatXOffset + 0.5f);
  int YOffset = (int)(FloatYOffset + 0.5f);
  int iWhidth = (int)ceil(FloatGLWidth);
  int iHeight = (int)ceil(FloatGLHeight);
  iWhidth -=
      iWhidth % 4;  // ensure divisibility by 4 to make it compatible with all the video encoders
  iHeight -= iHeight % 4;

  target_rc.left = XOffset;
  target_rc.top = YOffset;
  target_rc.right = XOffset + iWhidth;
  target_rc.bottom = YOffset + iHeight;
}

void Renderer::SetWindowSize(int width, int height)
{
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  // Scale the window size by the EFB scale.
  CalculateTargetScale(width, height, &width, &height);

  float scaled_width, scaled_height;
  std::tie(scaled_width, scaled_height) = ScaleToDisplayAspectRatio(width, height);

  if (g_ActiveConfig.bCrop)
  {
    // Force 4:3 or 16:9 by cropping the image.
    float current_aspect = scaled_width / scaled_height;
    float expected_aspect =
        (g_ActiveConfig.iAspectRatio == ASPECT_ANALOG_WIDE ||
         (g_ActiveConfig.iAspectRatio != ASPECT_ANALOG && Core::g_aspect_wide)) ?
            (16.0f / 9.0f) :
            (4.0f / 3.0f);
    if (current_aspect > expected_aspect)
    {
      // keep height, crop width
      scaled_width = scaled_height * expected_aspect;
    }
    else
    {
      // keep width, crop height
      scaled_height = scaled_width / expected_aspect;
    }
  }

  width = static_cast<int>(std::ceil(scaled_width));
  height = static_cast<int>(std::ceil(scaled_height));

  // UpdateDrawRectangle() makes sure that the rendered image is divisible by four for video
  // encoders, so do that here too to match it
  width -= width % 4;
  height -= height % 4;

  Host_RequestRenderWindowSize(width, height);
}

void Renderer::CheckFifoRecording()
{
  bool wasRecording = g_bRecordFifoData;
  g_bRecordFifoData = FifoRecorder::GetInstance().IsRecording();

  if (g_bRecordFifoData)
  {
    if (!wasRecording)
    {
      RecordVideoMemory();
    }

    FifoRecorder::GetInstance().EndFrame(CommandProcessor::fifo.CPBase,
                                         CommandProcessor::fifo.CPEnd);
  }
}

void Renderer::RecordVideoMemory()
{
  const u32* bpmem_ptr = reinterpret_cast<const u32*>(&bpmem);
  u32 cpmem[256] = {};
  // The FIFO recording format splits XF memory into xfmem and xfregs; follow
  // that split here.
  const u32* xfmem_ptr = reinterpret_cast<const u32*>(&xfmem);
  const u32* xfregs_ptr = reinterpret_cast<const u32*>(&xfmem) + FifoDataFile::XF_MEM_SIZE;
  u32 xfregs_size = sizeof(XFMemory) / 4 - FifoDataFile::XF_MEM_SIZE;

  FillCPMemoryArray(cpmem);

  FifoRecorder::GetInstance().SetVideoMemory(bpmem_ptr, cpmem, xfmem_ptr, xfregs_ptr, xfregs_size,
                                             texMem);
}

void Renderer::Swap(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc,
                    u64 ticks, float Gamma)
{
  // TODO: merge more generic parts into VideoCommon
  g_renderer->SwapImpl(xfbAddr, fbWidth, fbStride, fbHeight, rc, ticks, Gamma);

  if (XFBWrited)
    g_renderer->m_fps_counter.Update();

  frameCount++;
  GFX_DEBUGGER_PAUSE_AT(NEXT_FRAME, true);

  // Begin new frame
  // Set default viewport and scissor, for the clear to work correctly
  // New frame
  stats.ResetFrame();

  Core::Callback_VideoCopiedToXFB(XFBWrited ||
                                  (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB));
  XFBWrited = false;
}

bool Renderer::IsFrameDumping()
{
  if (s_screenshot.IsSet())
    return true;

#if defined(HAVE_LIBAV) || defined(_WIN32)
  if (SConfig::GetInstance().m_DumpFrames)
    return true;
#endif

  ShutdownFrameDumping();
  return false;
}

void Renderer::ShutdownFrameDumping()
{
  if (!m_frame_dump_thread_running.IsSet())
    return;

  FinishFrameData();
  m_frame_dump_thread_running.Clear();
  m_frame_dump_start.Set();
}

void Renderer::DumpFrameData(const u8* data, int w, int h, int stride, const AVIDump::Frame& state,
                             bool swap_upside_down)
{
  FinishFrameData();

  m_frame_dump_config = FrameDumpConfig{data, w, h, stride, swap_upside_down, state};

  if (!m_frame_dump_thread_running.IsSet())
  {
    if (m_frame_dump_thread.joinable())
      m_frame_dump_thread.join();
    m_frame_dump_thread_running.Set();
    m_frame_dump_thread = std::thread(&Renderer::RunFrameDumps, this);
  }

  m_frame_dump_start.Set();
  m_frame_dump_frame_running = true;
}

void Renderer::FinishFrameData()
{
  if (!m_frame_dump_frame_running)
    return;

  m_frame_dump_done.Wait();
  m_frame_dump_frame_running = false;
}

void Renderer::RunFrameDumps()
{
  Common::SetCurrentThreadName("FrameDumping");
  bool dump_to_avi = !g_ActiveConfig.bDumpFramesAsImages;
  bool frame_dump_started = false;

// If Dolphin was compiled without libav, we only support dumping to images.
#if !defined(HAVE_LIBAV) && !defined(_WIN32)
  if (dump_to_avi)
  {
    WARN_LOG(VIDEO, "AVI frame dump requested, but Dolphin was compiled without libav. "
                    "Frame dump will be saved as images instead.");
    dump_to_avi = false;
  }
#endif

  while (true)
  {
    m_frame_dump_start.Wait();
    if (!m_frame_dump_thread_running.IsSet())
      break;

    auto config = m_frame_dump_config;

    if (config.upside_down)
    {
      config.data = config.data + (config.height - 1) * config.stride;
      config.stride = -config.stride;
    }

    // Save screenshot
    if (s_screenshot.TestAndClear())
    {
      std::lock_guard<std::mutex> lk(s_criticalScreenshot);

      if (TextureToPng(config.data, config.stride, s_sScreenshotName, config.width, config.height,
                       false))
        OSD::AddMessage("Screenshot saved to " + s_sScreenshotName);

      // Reset settings
      s_sScreenshotName.clear();
      s_screenshotCompleted.Set();
    }

    if (SConfig::GetInstance().m_DumpFrames)
    {
      if (!frame_dump_started)
      {
        if (dump_to_avi)
          frame_dump_started = StartFrameDumpToAVI(config);
        else
          frame_dump_started = StartFrameDumpToImage(config);

        // Stop frame dumping if we fail to start.
        if (!frame_dump_started)
          SConfig::GetInstance().m_DumpFrames = false;
      }

      // If we failed to start frame dumping, don't write a frame.
      if (frame_dump_started)
      {
        if (dump_to_avi)
          DumpFrameToAVI(config);
        else
          DumpFrameToImage(config);
      }
    }

    m_frame_dump_done.Set();
  }

  if (frame_dump_started)
  {
    // No additional cleanup is needed when dumping to images.
    if (dump_to_avi)
      StopFrameDumpToAVI();
  }
}

#if defined(HAVE_LIBAV) || defined(_WIN32)

bool Renderer::StartFrameDumpToAVI(const FrameDumpConfig& config)
{
  return AVIDump::Start(config.width, config.height);
}

void Renderer::DumpFrameToAVI(const FrameDumpConfig& config)
{
  AVIDump::AddFrame(config.data, config.width, config.height, config.stride, config.state);
}

void Renderer::StopFrameDumpToAVI()
{
  AVIDump::Stop();
}

#else

bool Renderer::StartFrameDumpToAVI(const FrameDumpConfig& config)
{
  return false;
}

void Renderer::DumpFrameToAVI(const FrameDumpConfig& config)
{
}

void Renderer::StopFrameDumpToAVI()
{
}

#endif  // defined(HAVE_LIBAV) || defined(WIN32)

std::string Renderer::GetFrameDumpNextImageFileName() const
{
  return StringFromFormat("%sframedump_%u.png", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(),
                          m_frame_dump_image_counter);
}

bool Renderer::StartFrameDumpToImage(const FrameDumpConfig& config)
{
  m_frame_dump_image_counter = 1;
  if (!SConfig::GetInstance().m_DumpFramesSilent)
  {
    // Only check for the presence of the first image to confirm overwriting.
    // A previous run will always have at least one image, and it's safe to assume that if the user
    // has allowed the first image to be overwritten, this will apply any remaining images as well.
    std::string filename = GetFrameDumpNextImageFileName();
    if (File::Exists(filename))
    {
      if (!AskYesNoT("Frame dump image(s) '%s' already exists. Overwrite?", filename.c_str()))
        return false;
    }
  }

  return true;
}

void Renderer::DumpFrameToImage(const FrameDumpConfig& config)
{
  std::string filename = GetFrameDumpNextImageFileName();
  TextureToPng(config.data, config.stride, filename, config.width, config.height, false);
  m_frame_dump_image_counter++;
}
