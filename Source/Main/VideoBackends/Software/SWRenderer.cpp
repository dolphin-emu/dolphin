// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/SWRenderer.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/GL/GLContext.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/HW/Memmap.h"

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWTexture.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

SWRenderer::SWRenderer(std::unique_ptr<SWOGLWindow> window)
    : ::Renderer(static_cast<int>(MAX_XFB_WIDTH), static_cast<int>(MAX_XFB_HEIGHT)),
      m_window(std::move(window))
{
}

bool SWRenderer::IsHeadless() const
{
  return m_window->IsHeadless();
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

std::unique_ptr<AbstractFramebuffer>
SWRenderer::CreateFramebuffer(const AbstractTexture* color_attachment,
                              const AbstractTexture* depth_attachment)
{
  return SW::SWFramebuffer::Create(static_cast<const SW::SWTexture*>(color_attachment),
                                   static_cast<const SW::SWTexture*>(depth_attachment));
}

void SWRenderer::RenderText(const std::string& pstr, int left, int top, u32 color)
{
  m_window->PrintText(pstr, left, top, color);
}

class SWShader final : public AbstractShader
{
public:
  explicit SWShader(ShaderStage stage) : AbstractShader(stage) {}
  ~SWShader() = default;

  bool HasBinary() const override { return false; }
  BinaryData GetBinary() const override { return {}; }
};

std::unique_ptr<AbstractShader>
SWRenderer::CreateShaderFromSource(ShaderStage stage, const char* source, size_t length)
{
  return std::make_unique<SWShader>(stage);
}

std::unique_ptr<AbstractShader> SWRenderer::CreateShaderFromBinary(ShaderStage stage,
                                                                   const void* data, size_t length)
{
  return std::make_unique<SWShader>(stage);
}

class SWPipeline final : public AbstractPipeline
{
public:
  SWPipeline() : AbstractPipeline() {}
  ~SWPipeline() override = default;
};

std::unique_ptr<AbstractPipeline> SWRenderer::CreatePipeline(const AbstractPipelineConfig& config)
{
  return std::make_unique<SWPipeline>();
}

// Called on the GPU thread
void SWRenderer::SwapImpl(AbstractTexture* texture, const EFBRectangle& xfb_region, u64 ticks)
{
  OSD::DoCallbacks(OSD::CallbackType::OnFrame);

  if (!IsHeadless())
  {
    DrawDebugText();
    m_window->ShowImage(texture, xfb_region);
  }

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
