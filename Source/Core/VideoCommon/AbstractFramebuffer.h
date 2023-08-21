// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/TextureConfig.h"

class AbstractTexture;

// An abstract framebuffer wraps a backend framebuffer/view object, which can be used to
// draw onto a texture. Currently, only single-level textures are supported. Multi-layer
// textures will render by default only to the first layer, however, multiple layers
// be rendered in parallel using geometry shaders and layer variable.

class AbstractFramebuffer
{
public:
  AbstractFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                      AbstractTextureFormat color_format, AbstractTextureFormat depth_format,
                      u32 width, u32 height, u32 layers, u32 samples);
  virtual ~AbstractFramebuffer();

  static bool ValidateConfig(const AbstractTexture* color_attachment,
                             const AbstractTexture* depth_attachment);

  AbstractTexture* GetColorAttachment() const { return m_color_attachment; }
  AbstractTexture* GetDepthAttachment() const { return m_depth_attachment; }
  AbstractTextureFormat GetColorFormat() const { return m_color_format; }
  AbstractTextureFormat GetDepthFormat() const { return m_depth_format; }
  bool HasColorBuffer() const { return m_color_format != AbstractTextureFormat::Undefined; }
  bool HasDepthBuffer() const { return m_depth_format != AbstractTextureFormat::Undefined; }
  u32 GetWidth() const { return m_width; }
  u32 GetHeight() const { return m_height; }
  u32 GetLayers() const { return m_layers; }
  u32 GetSamples() const { return m_samples; }
  MathUtil::Rectangle<int> GetRect() const;

protected:
  AbstractTexture* m_color_attachment;
  AbstractTexture* m_depth_attachment;
  AbstractTextureFormat m_color_format;
  AbstractTextureFormat m_depth_format;
  u32 m_width;
  u32 m_height;
  u32 m_layers;
  u32 m_samples;
};
