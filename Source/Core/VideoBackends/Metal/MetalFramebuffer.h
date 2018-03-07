// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/RenderBase.h"

namespace Metal
{
class MetalTexture;

class MetalFramebuffer : public AbstractFramebuffer
{
public:
  MetalFramebuffer(mtlpp::RenderPassDescriptor& load_rpd, mtlpp::RenderPassDescriptor& discard_rpd,
                   mtlpp::RenderPassDescriptor& clear_rpd, AbstractTextureFormat color_format,
                   AbstractTextureFormat depth_format, u32 width, u32 height, u32 layers,
                   u32 samples);
  ~MetalFramebuffer() override;

  const mtlpp::RenderPassDescriptor& GetLoadDescriptor() const { return m_load_rpd; }
  const mtlpp::RenderPassDescriptor& GetDiscardDescriptor() const { return m_discard_rpd; }
  const mtlpp::RenderPassDescriptor&
  GetClearDescriptor(const Renderer::ClearColor& clear_color = {}, float clear_depth = 1.0f) const;

  virtual bool UpdateSurface() { return true; }
  virtual void Present() {}
  static std::unique_ptr<MetalFramebuffer>
  CreateForAbstractTexture(const MetalTexture* color_attachment,
                           const MetalTexture* depth_attachment);
  static std::unique_ptr<MetalFramebuffer> CreateForTexture(const mtlpp::Texture& tex);
  static std::unique_ptr<MetalFramebuffer> CreateForWindow(void* window_handle,
                                                           AbstractTextureFormat format);

protected:
  mtlpp::RenderPassDescriptor m_load_rpd;
  mtlpp::RenderPassDescriptor m_discard_rpd;
  mtlpp::RenderPassDescriptor m_clear_rpd;

  mutable Renderer::ClearColor m_last_clear_color = {};
  mutable float m_last_clear_depth = 1.0f;
};

}  // namespace Metal
