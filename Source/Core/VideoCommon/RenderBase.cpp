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

#include <cinttypes>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Profiler.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/VideoInterface.h"
#include "Core/Host.h"
#include "Core/Movie.h"

#include "OnScreenDisplay.h"
#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

// TODO: Move these out of here.
int frameCount;

std::unique_ptr<Renderer> g_renderer;

std::mutex Renderer::s_criticalScreenshot;
std::string Renderer::s_sScreenshotName;

Common::Event Renderer::s_screenshotCompleted;

volatile bool Renderer::s_bScreenshot;

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
  TextureCacheBase::OnConfigChanged(g_ActiveConfig);
}

Renderer::~Renderer()
{
  // invalidate previous efb format
  prev_efb_format = PEControl::INVALID_FMT;

  efb_scale_numeratorX = efb_scale_numeratorY = efb_scale_denominatorX = efb_scale_denominatorY = 1;

#if defined(HAVE_LIBAV) || defined(_WIN32)
  // Stop frame dumping if it was left enabled at shutdown time.
  if (m_AVI_dumping)
  {
    AVIDump::Stop();
    m_AVI_dumping = false;
  }
#endif
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
    // below div two to convert from bytes to pixels - it expects width, not stride
    Swap(xfbAddr, fbStride / 2, fbStride / 2, fbHeight, sourceRc, Gamma);
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
bool Renderer::CalculateTargetSize(unsigned int framebuffer_width, unsigned int framebuffer_height)
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
  s_bScreenshot = true;
}

void Renderer::DrawDebugText()
{
  auto draw_text = [](OSD::MessageType type, const std::string& message) {
    OSD::AddTypedMessage(type, message, OSD::Duration::SHORT, OSD::Color::CYAN);
  };

  if (g_ActiveConfig.bShowFPS)
  {
    draw_text(OSD::MessageType::FPS,
              StringFromFormat("FPS: %u", g_renderer->m_fps_counter.GetFPS()));
  }

  if (SConfig::GetInstance().m_ShowFrameCount)
  {
    draw_text(OSD::MessageType::FrameCount,
              StringFromFormat("Frame: %" PRIu64, Movie::GetCurrentFrame()));

    if (Movie::IsPlayingInput())
    {
      draw_text(OSD::MessageType::MovieInputCount,
                StringFromFormat("Input: %" PRIu64 " / %" PRIu64, Movie::GetCurrentInputCount(),
                                 Movie::GetTotalInputCount()));
    }
  }

  if (SConfig::GetInstance().m_ShowLag)
  {
    draw_text(OSD::MessageType::MovieLag,
              StringFromFormat("Lag: %" PRIu64, Movie::GetCurrentLagCount()));
  }

  if (SConfig::GetInstance().m_ShowInputDisplay)
  {
    draw_text(OSD::MessageType::MovieInput, Movie::GetInputDisplay());
  }

  if (SConfig::GetInstance().m_ShowRTC)
  {
    draw_text(OSD::MessageType::RTC, Movie::GetRTCDisplay());
  }
}

void Renderer::UpdateDrawRectangle(int backbuffer_width, int backbuffer_height)
{
  float FloatGLWidth = (float)backbuffer_width;
  float FloatGLHeight = (float)backbuffer_height;
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
  float Ratio;
  if (g_ActiveConfig.iAspectRatio == ASPECT_ANALOG_WIDE ||
      (g_ActiveConfig.iAspectRatio != ASPECT_ANALOG && Core::g_aspect_wide))
  {
    Ratio = (WinWidth / WinHeight) / AspectToWidescreen(VideoInterface::GetAspectRatio());
  }
  else
  {
    Ratio = (WinWidth / WinHeight) / VideoInterface::GetAspectRatio();
  }

  if (g_ActiveConfig.iAspectRatio != ASPECT_STRETCH)
  {
    if (Ratio > 1.0f)
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

  FifoRecorder::GetInstance().SetVideoMemory(bpmem_ptr, cpmem, xfmem_ptr, xfregs_ptr, xfregs_size);
}

void Renderer::Swap(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc,
                    float Gamma)
{
  // TODO: merge more generic parts into VideoCommon
  g_renderer->SwapImpl(xfbAddr, fbWidth, fbStride, fbHeight, rc, Gamma);

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
  if (s_bScreenshot)
    return true;

#if defined(HAVE_LIBAV) || defined(_WIN32)
  if (SConfig::GetInstance().m_DumpFrames)
    return true;

  if (m_last_frame_dumped && m_AVI_dumping)
  {
    AVIDump::Stop();
    std::vector<u8>().swap(m_frame_data);
    m_AVI_dumping = false;
    OSD::AddMessage("Stop dumping frames", 2000);
  }
  m_last_frame_dumped = false;
#endif
  return false;
}

void Renderer::DumpFrameData(const u8* data, int w, int h, int stride, bool swap_upside_down)
{
  if (w == 0 || h == 0)
    return;

  // TODO: Refactor this. Right now it's needed for the implace flipping of the image.
  m_frame_data.assign(data, data + stride * h);
  if (swap_upside_down)
    FlipImageData(m_frame_data.data(), w, h, 4);

  // Save screenshot
  if (s_bScreenshot)
  {
    std::lock_guard<std::mutex> lk(s_criticalScreenshot);

    if (TextureToPng(m_frame_data.data(), stride, s_sScreenshotName, w, h, false))
      OSD::AddMessage("Screenshot saved to " + s_sScreenshotName);

    // Reset settings
    s_sScreenshotName.clear();
    s_bScreenshot = false;
    s_screenshotCompleted.Set();
  }

#if defined(HAVE_LIBAV) || defined(_WIN32)
  if (SConfig::GetInstance().m_DumpFrames)
  {
    if (!m_last_frame_dumped)
    {
      m_AVI_dumping = AVIDump::Start(w, h);
      if (!m_AVI_dumping)
      {
        OSD::AddMessage("AVIDump Start failed", 2000);
      }
      else
      {
        OSD::AddMessage(StringFromFormat("Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)",
                                         File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), w, h),
                        2000);
      }
    }
    if (m_AVI_dumping)
    {
      AVIDump::AddFrame(m_frame_data.data(), w, h, stride);
    }

    m_last_frame_dumped = true;
  }
#endif
}

void Renderer::FinishFrameData()
{
}

void Renderer::FlipImageData(u8* data, int w, int h, int pixel_width)
{
  for (int y = 0; y < h / 2; ++y)
  {
    for (int x = 0; x < w; ++x)
    {
      for (int delta = 0; delta < pixel_width; ++delta)
        std::swap(data[(y * w + x) * pixel_width + delta],
                  data[((h - 1 - y) * w + x) * pixel_width + delta]);
    }
  }
}
