// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"

void SWRenderer::Init()
{
}

void SWRenderer::Shutdown()
{
  g_Config.bRunning = false;
  UpdateActiveConfig();
}

void SWRenderer::RenderText(const std::string& pstr, int left, int top, u32 color)
{
  SWOGLWindow::s_instance->PrintText(pstr, left, top, color);
}

// Called on the GPU thread
void SWRenderer::SwapImpl(AbstractTextureBase* texture, const EFBRectangle& rc, float Gamma)
{
  if (!Fifo::WillSkipCurrentFrame())
  {
    SWOGLWindow::s_instance->ShowImage(texture, 1.0);

    // Save screenshot
    if (s_bScreenshot)
    {
      std::lock_guard<std::mutex> lk(s_criticalScreenshot);

      if (texture->Save(s_sScreenshotName, 0))
        OSD::AddMessage("Screenshot saved to " + s_sScreenshotName);

      // Reset settings
      s_sScreenshotName.clear();
      s_bScreenshot = false;
      s_screenshotCompleted.Set();
    }

    if (SConfig::GetInstance().m_DumpFrames)
    {
      static int frame_index = 0;
      texture->Save(StringFromFormat("%sframe%i_color.png",
                                     File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), frame_index),
                    0);
      frame_index++;
    }
  }

  OSD::DoCallbacks(OSD::CallbackType::OnFrame);

  DrawDebugText();

  SWOGLWindow::s_instance->ShowImage(texture, 1.0);

  UpdateActiveConfig();

  // virtual XFB is not supported
  if (g_ActiveConfig.bUseXFB)
    g_ActiveConfig.bUseRealXFB = true;
}

u32 SWRenderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
  u32 value = 0;

  switch (type)
  {
  case PEEK_Z:
  {
    value = EfbInterface::GetDepth(x, y);
    break;
  }
  case PEEK_COLOR:
  {
    u32 color = 0;
    EfbInterface::GetColor(x, y, (u8*)&color);

    // rgba to argb
    value = (color >> 8) | (color & 0xff) << 24;
    break;
  }
  default:
    break;
  }

  return value;
}

u16 SWRenderer::BBoxRead(int index)
{
  return BoundingBox::coords[index];
}

void SWRenderer::BBoxWrite(int index, u16 value)
{
  BoundingBox::coords[index] = value;
}

TargetRectangle SWRenderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
  TargetRectangle result;
  result.left = rc.left;
  result.top = rc.top;
  result.right = rc.right;
  result.bottom = rc.bottom;
  return result;
}

void SWRenderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable,
                             bool zEnable, u32 color, u32 z)
{
  EfbCopy::ClearEfb();
}
