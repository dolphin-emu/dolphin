// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/SWRenderer.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/GL/GLContext.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWTexture.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"

namespace SW
{
SWRenderer::SWRenderer(std::unique_ptr<SWOGLWindow> window)
    : ::Renderer(static_cast<int>(MAX_XFB_WIDTH), static_cast<int>(MAX_XFB_HEIGHT), 1.0f,
                 AbstractTextureFormat::RGBA8),
      m_window(std::move(window))
{
}

bool SWRenderer::IsHeadless() const
{
  return m_window->IsHeadless();
}

std::unique_ptr<AbstractTexture> SWRenderer::CreateTexture(const TextureConfig& config)
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

class SWShader final : public AbstractShader
{
public:
  explicit SWShader(ShaderStage stage) : AbstractShader(stage) {}
  ~SWShader() = default;

  BinaryData GetBinary() const override { return {}; }
};

std::unique_ptr<AbstractShader>
SWRenderer::CreateShaderFromSource(ShaderStage stage, [[maybe_unused]] std::string_view source)
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
}  // namespace SW
