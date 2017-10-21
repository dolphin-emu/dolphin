// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/SWRenderer.h"

#include <string>

#include "Common/CommonTypes.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/HW/Memmap.h"

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWTexture.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

SWRenderer::SWRenderer()
    : ::Renderer(static_cast<int>(MAX_XFB_WIDTH), static_cast<int>(MAX_XFB_HEIGHT))
{
}

void SWRenderer::Init()
{
}

void SWRenderer::Shutdown()
{
  UpdateActiveConfig();
}

std::unique_ptr<AbstractTexture> SWRenderer::CreateTexture(const TextureConfig& config)
{
  return std::make_unique<SW::SWTexture>(config);
}

std::unique_ptr<AbstractStagingTexture>
SWRenderer::CreateStagingTexture(StagingTextureType type, const TextureConfig& config)
{
  return std::make_unique<SW::SWStagingTexture>(type, config);
}

void SWRenderer::RenderText(const std::string& pstr, int left, int top, u32 color)
{
  SWOGLWindow::s_instance->PrintText(pstr, left, top, color);
}

// Called on the GPU thread
void SWRenderer::SwapImpl(AbstractTexture* texture, const EFBRectangle& xfb_region, u64 ticks,
                          float Gamma)
{
  OSD::DoCallbacks(OSD::CallbackType::OnFrame);

  DrawDebugText();

  SWOGLWindow::s_instance->ShowImage(texture, xfb_region);

  UpdateActiveConfig();
}

u32 SWRenderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
  u32 value = 0;

  switch (type)
  {
  case EFBAccessType::PeekZ:
  {
    value = EfbInterface::GetDepth(x, y);
    break;
  }
  case EFBAccessType::PeekColor:
  {
    const u32 color = EfbInterface::GetColor(x, y);

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
