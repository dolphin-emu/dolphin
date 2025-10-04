// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Common/GL/GLUtil.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"

namespace OGL
{
class OGLTexture final : public AbstractTexture
{
public:
  explicit OGLTexture(const TextureConfig& tex_config, std::string_view name);
  ~OGLTexture() override;

  void CopyRectangleFromTexture(const AbstractTexture* src,
      const MathUtil::Rectangle<int>& src_rect, u32 src_layer, u32 src_level,
      const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer, u32 dst_level) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
      u32 layer, u32 level) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer, size_t buffer_size,
      u32 layer) override;

  GLuint GetGLTextureId() const { return m_texId; }
  GLenum GetGLTarget() const
  {
    if (m_config.type == AbstractTextureType::Texture_2DArray)
    {
      if (m_config.IsMultisampled())
        return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
      else
        return GL_TEXTURE_2D_ARRAY;
    }
    else if (m_config.type == AbstractTextureType::Texture_2D)
    {
      if (m_config.IsMultisampled())
        return GL_TEXTURE_2D_MULTISAMPLE;
      else
        return GL_TEXTURE_2D;
    }
    else if (m_config.type == AbstractTextureType::Texture_CubeMap)
    {
      return GL_TEXTURE_CUBE_MAP;
    }

    return GL_TEXTURE_2D_ARRAY;
  }
  static GLenum GetGLInternalFormatForTextureFormat(AbstractTextureFormat format, bool storage);
  GLenum GetGLFormatForImageTexture() const;

private:
  void BlitFramebuffer(OGLTexture* srcentry, const MathUtil::Rectangle<int>& src_rect,
      u32 src_layer, u32 src_level, const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
      u32 dst_level);

  GLuint m_texId;
  std::string m_name;
};

class OGLStagingTexture final : public AbstractStagingTexture
{
public:
  OGLStagingTexture() = delete;
  ~OGLStagingTexture() override;

  void CopyFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& src_rect,
      u32 src_layer, u32 src_level, const MathUtil::Rectangle<int>& dst_rect) override;
  void CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
      const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer, u32 dst_level) override;

  bool Map() override;
  void Unmap() override;
  void Flush() override;

  static std::unique_ptr<OGLStagingTexture> Create(
      StagingTextureType type, const TextureConfig& config);

private:
  OGLStagingTexture(StagingTextureType type, const TextureConfig& config, GLenum target,
      GLuint buffer_name, size_t buffer_size, char* map_ptr, size_t map_stride);

private:
  GLenum m_target;
  GLuint m_buffer_name;
  size_t m_buffer_size;
  GLsync m_fence = nullptr;
};

class OGLFramebuffer final : public AbstractFramebuffer
{
public:
  OGLFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
      std::vector<AbstractTexture*> additional_color_attachments,
      AbstractTextureFormat color_format, AbstractTextureFormat depth_format, u32 width, u32 height,
      u32 layers, u32 samples, GLuint fbo);
  ~OGLFramebuffer() override;

  static std::unique_ptr<OGLFramebuffer> Create(OGLTexture* color_attachment,
      OGLTexture* depth_attachment, std::vector<AbstractTexture*> additional_color_attachments);

  GLuint GetFBO() const { return m_fbo; }

  // Used for updating the dimensions of the system/window framebuffer.
  void UpdateDimensions(u32 width, u32 height);

protected:
  GLuint m_fbo;
};

}  // namespace OGL
