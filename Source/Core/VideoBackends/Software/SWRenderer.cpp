// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/SWRenderer.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/GL/GLContext.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWBoundingBox.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWTexture.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"

namespace SW
{
SWRenderer::SWRenderer(std::unique_ptr<SWOGLWindow> window)
    : ::Renderer(static_cast<int>(std::max(window->GetContext()->GetBackBufferWidth(), 1u)),
                 static_cast<int>(std::max(window->GetContext()->GetBackBufferHeight(), 1u)), 1.0f,
                 AbstractTextureFormat::RGBA8),
      m_window(std::move(window))
{
}

bool SWRenderer::IsHeadless() const
{
  return m_window->IsHeadless();
}

std::unique_ptr<AbstractTexture> SWRenderer::CreateTexture(const TextureConfig& config,
                                                           [[maybe_unused]] std::string_view name)
{
  return std::make_unique<SWTexture>(config);
}

std::unique_ptr<AbstractStagingTexture>
SWRenderer::CreateStagingTexture(StagingTextureType type, const TextureConfig& config)
{
  return std::make_unique<SWStagingTexture>(type, config);
}

std::unique_ptr<AbstractFramebuffer>
SWRenderer::CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment)
{
  return SWFramebuffer::Create(static_cast<SWTexture*>(color_attachment),
                               static_cast<SWTexture*>(depth_attachment));
}

void SWRenderer::BindBackbuffer(const ClearColor& clear_color)
{
  // Look for framebuffer resizes
  if (!m_surface_resized.TestAndClear())
    return;

  GLContext* context = m_window->GetContext();
  context->Update();
  m_backbuffer_width = context->GetBackBufferWidth();
  m_backbuffer_height = context->GetBackBufferHeight();
}

class SWShader final : public AbstractShader
{
public:
  explicit SWShader(ShaderStage stage) : AbstractShader(stage) {}
  ~SWShader() = default;

  BinaryData GetBinary() const override { return {}; }
};

std::unique_ptr<AbstractShader>
SWRenderer::CreateShaderFromSource(ShaderStage stage, [[maybe_unused]] std::string_view source,
                                   [[maybe_unused]] std::string_view name)
{
  return std::make_unique<SWShader>(stage);
}

std::unique_ptr<AbstractShader>
SWRenderer::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t length,
                                   [[maybe_unused]] std::string_view name)
{
  return std::make_unique<SWShader>(stage);
}

class SWPipeline final : public AbstractPipeline
{
public:
  SWPipeline() = default;
  ~SWPipeline() override = default;
};

std::unique_ptr<AbstractPipeline> SWRenderer::CreatePipeline(const AbstractPipelineConfig& config,
                                                             const void* cache_data,
                                                             size_t cache_data_length)
{
  return std::make_unique<SWPipeline>();
}

// Called on the GPU thread
void SWRenderer::RenderXFBToScreen(const MathUtil::Rectangle<int>& target_rc,
                                   const AbstractTexture* source_texture,
                                   const MathUtil::Rectangle<int>& source_rc)
{
  if (!IsHeadless())
    m_window->ShowImage(source_texture, source_rc);
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

    // check what to do with the alpha channel (GX_PokeAlphaRead)
    PixelEngine::AlphaReadMode alpha_read_mode = PixelEngine::GetAlphaReadMode();

    if (alpha_read_mode == PixelEngine::AlphaReadMode::ReadNone)
    {
      // value is OK as it is
    }
    else if (alpha_read_mode == PixelEngine::AlphaReadMode::ReadFF)
    {
      value |= 0xFF000000;
    }
    else
    {
      if (alpha_read_mode != PixelEngine::AlphaReadMode::Read00)
      {
        PanicAlertFmt("Invalid PE alpha read mode: {}", static_cast<u16>(alpha_read_mode));
      }
      value &= 0x00FFFFFF;
    }

    break;
  }
  default:
    break;
  }

  return value;
}

std::unique_ptr<BoundingBox> SWRenderer::CreateBoundingBox() const
{
  return std::make_unique<SWBoundingBox>();
}

void SWRenderer::ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                             bool zEnable, u32 color, u32 z)
{
  EfbCopy::ClearEfb();
}

std::unique_ptr<NativeVertexFormat>
SWRenderer::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<NativeVertexFormat>(vtx_decl);
}

void SWRenderer::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  // BPFunctions calls SetScissorRect with the "best" scissor rect whenever the viewport or scissor
  // changes.  However, the software renderer is actually able to use multiple scissor rects (which
  // is necessary in a few renderering edge cases, such as with Major Minor's Majestic March).
  // Thus, we use this as a signal to update the list of scissor rects, but ignore the parameter.
  Rasterizer::ScissorChanged();
}
}  // namespace SW
