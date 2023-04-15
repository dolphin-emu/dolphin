// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/SWGfx.h"

#include "Common/GL/GLContext.h"

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWTexture.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/Present.h"

namespace SW
{
SWGfx::SWGfx(std::unique_ptr<SWOGLWindow> window) : m_window(std::move(window))
{
}

bool SWGfx::IsHeadless() const
{
  return m_window->IsHeadless();
}

bool SWGfx::SupportsUtilityDrawing() const
{
  return false;
}

std::unique_ptr<AbstractTexture> SWGfx::CreateTexture(const TextureConfig& config,
                                                      [[maybe_unused]] std::string_view name)
{
  return std::make_unique<SWTexture>(config);
}

std::unique_ptr<AbstractStagingTexture> SWGfx::CreateStagingTexture(StagingTextureType type,
                                                                    const TextureConfig& config)
{
  return std::make_unique<SWStagingTexture>(type, config);
}

std::unique_ptr<AbstractFramebuffer> SWGfx::CreateFramebuffer(AbstractTexture* color_attachment,
                                                              AbstractTexture* depth_attachment)
{
  return SWFramebuffer::Create(static_cast<SWTexture*>(color_attachment),
                               static_cast<SWTexture*>(depth_attachment));
}

void SWGfx::BindBackbuffer(const ClearColor& clear_color)
{
  // Look for framebuffer resizes
  if (!g_presenter->SurfaceResizedTestAndClear())
    return;

  GLContext* context = m_window->GetContext();
  context->Update();
  g_presenter->SetBackbuffer(context->GetBackBufferWidth(), context->GetBackBufferHeight());
}

class SWShader final : public AbstractShader
{
public:
  explicit SWShader(ShaderStage stage) : AbstractShader(stage) {}
  ~SWShader() = default;

  BinaryData GetBinary() const override { return {}; }
};

std::unique_ptr<AbstractShader>
SWGfx::CreateShaderFromSource(ShaderStage stage, [[maybe_unused]] std::string_view source,
                              [[maybe_unused]] std::string_view name)
{
  return std::make_unique<SWShader>(stage);
}

std::unique_ptr<AbstractShader>
SWGfx::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t length,
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

std::unique_ptr<AbstractPipeline> SWGfx::CreatePipeline(const AbstractPipelineConfig& config,
                                                        const void* cache_data,
                                                        size_t cache_data_length)
{
  return std::make_unique<SWPipeline>();
}

// Called on the GPU thread
void SWGfx::ShowImage(const AbstractTexture* source_texture,
                      const MathUtil::Rectangle<int>& source_rc)
{
  if (!IsHeadless())
    m_window->ShowImage(source_texture, source_rc);
}

void SWGfx::ClearRegion(const MathUtil::Rectangle<int>& target_rc, bool colorEnable,
                        bool alphaEnable, bool zEnable, u32 color, u32 z)
{
  EfbCopy::ClearEfb();
}

std::unique_ptr<NativeVertexFormat>
SWGfx::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<NativeVertexFormat>(vtx_decl);
}

void SWGfx::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  // BPFunctions calls SetScissorRect with the "best" scissor rect whenever the viewport or scissor
  // changes.  However, the software renderer is actually able to use multiple scissor rects (which
  // is necessary in a few renderering edge cases, such as with Major Minor's Majestic March).
  // Thus, we use this as a signal to update the list of scissor rects, but ignore the parameter.
  Rasterizer::ScissorChanged();
}

SurfaceInfo SWGfx::GetSurfaceInfo() const
{
  GLContext* context = m_window->GetContext();
  return {std::max(context->GetBackBufferWidth(), 1u), std::max(context->GetBackBufferHeight(), 1u),
          1.0f, AbstractTextureFormat::RGBA8};
}

}  // namespace SW
