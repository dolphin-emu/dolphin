// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractTexture.h"

AbstractFramebuffer::AbstractFramebuffer(AbstractTexture* color_attachment,
                                         AbstractTexture* depth_attachment,
                                         AbstractTextureFormat color_format,
                                         AbstractTextureFormat depth_format, u32 width, u32 height,
                                         u32 layers, u32 samples)
    : m_color_attachment(color_attachment), m_depth_attachment(depth_attachment),
      m_color_format(color_format), m_depth_format(depth_format), m_width(width), m_height(height),
      m_layers(layers), m_samples(samples)
{
}

AbstractFramebuffer::~AbstractFramebuffer() = default;

bool AbstractFramebuffer::ValidateConfig(const AbstractTexture* color_attachment,
                                         const AbstractTexture* depth_attachment)
{
  // Must have at least a color or depth attachment.
  if (!color_attachment && !depth_attachment)
    return false;

  // Currently we only expose a single mip level for render target textures.
  // MSAA textures are not supported with mip levels on most backends, and it simplifies our
  // handling of framebuffers.
  auto CheckAttachment = [](const AbstractTexture* tex) {
    return tex->GetConfig().IsRenderTarget() && tex->GetConfig().levels == 1;
  };
  if ((color_attachment && !CheckAttachment(color_attachment)) ||
      (depth_attachment && !CheckAttachment(depth_attachment)))
  {
    return false;
  }

  // If both color and depth are present, their attributes must match.
  if (color_attachment && depth_attachment)
  {
    if (color_attachment->GetConfig().width != depth_attachment->GetConfig().width ||
        color_attachment->GetConfig().height != depth_attachment->GetConfig().height ||
        color_attachment->GetConfig().layers != depth_attachment->GetConfig().layers ||
        color_attachment->GetConfig().samples != depth_attachment->GetConfig().samples)
    {
      return false;
    }
  }

  return true;
}

MathUtil::Rectangle<int> AbstractFramebuffer::GetRect() const
{
  return MathUtil::Rectangle<int>(0, 0, static_cast<int>(m_width), static_cast<int>(m_height));
}
