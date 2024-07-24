// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/OGL/OGLTexture.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/OGL/OGLConfig.h"
#include "VideoBackends/OGL/OGLGfx.h"
#include "VideoBackends/OGL/SamplerCache.h"

#include "VideoCommon/VideoConfig.h"

namespace OGL
{
GLenum OGLTexture::GetGLInternalFormatForTextureFormat(AbstractTextureFormat format, bool storage)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
  case AbstractTextureFormat::DXT3:
    return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
  case AbstractTextureFormat::DXT5:
    return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
  case AbstractTextureFormat::BPTC:
    return GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
  case AbstractTextureFormat::RGBA8:
    return storage ? GL_RGBA8 : GL_RGBA;
  case AbstractTextureFormat::BGRA8:
    return storage ? GL_RGBA8 : GL_BGRA;
  case AbstractTextureFormat::RGB10_A2:
    return GL_RGB10_A2;
  case AbstractTextureFormat::RGBA16F:
    return GL_RGBA16F;
  case AbstractTextureFormat::R16:
    return GL_R16;
  case AbstractTextureFormat::R32F:
    return GL_R32F;
  case AbstractTextureFormat::D16:
    return GL_DEPTH_COMPONENT16;
  case AbstractTextureFormat::D24_S8:
    return GL_DEPTH24_STENCIL8;
  case AbstractTextureFormat::D32F:
    return GL_DEPTH_COMPONENT32F;
  case AbstractTextureFormat::D32F_S8:
    return GL_DEPTH32F_STENCIL8;
  default:
    PanicAlertFmt("Unhandled texture format.");
    return storage ? GL_RGBA8 : GL_RGBA;
  }
}

namespace
{
GLenum GetGLFormatForTextureFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::RGBA8:
    return GL_RGBA;
  case AbstractTextureFormat::BGRA8:
    return GL_BGRA;
  case AbstractTextureFormat::RGB10_A2:
    return GL_RGB10_A2;
  case AbstractTextureFormat::RGBA16F:
    return GL_RGBA16F;
  case AbstractTextureFormat::R16:
  case AbstractTextureFormat::R32F:
    return GL_RED;
  case AbstractTextureFormat::D16:
  case AbstractTextureFormat::D32F:
    return GL_DEPTH_COMPONENT;
  case AbstractTextureFormat::D24_S8:
  case AbstractTextureFormat::D32F_S8:
    return GL_DEPTH_STENCIL;
  // Compressed texture formats don't use this parameter.
  default:
    return GL_UNSIGNED_BYTE;
  }
}

GLenum GetGLTypeForTextureFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::RGBA8:
  case AbstractTextureFormat::BGRA8:
    return GL_UNSIGNED_BYTE;
  case AbstractTextureFormat::RGB10_A2:
    return GL_UNSIGNED_INT_2_10_10_10_REV;
  case AbstractTextureFormat::RGBA16F:
    return GL_HALF_FLOAT;
  case AbstractTextureFormat::R16:
    return GL_UNSIGNED_SHORT;
  case AbstractTextureFormat::R32F:
    return GL_FLOAT;
  case AbstractTextureFormat::D16:
    return GL_UNSIGNED_SHORT;
  case AbstractTextureFormat::D24_S8:
    return GL_UNSIGNED_INT_24_8;
  case AbstractTextureFormat::D32F:
    return GL_FLOAT;
  case AbstractTextureFormat::D32F_S8:
    return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
  // Compressed texture formats don't use this parameter.
  default:
    return GL_UNSIGNED_BYTE;
  }
}

bool UsePersistentStagingBuffers()
{
  // We require ARB_buffer_storage to create the persistent mapped buffer,
  // ARB_shader_image_load_store for glMemoryBarrier, and ARB_sync to ensure
  // the GPU has finished the copy before reading the buffer from the CPU.
  return g_ogl_config.bSupportsGLBufferStorage && g_ogl_config.bSupportsImageLoadStore &&
         g_ogl_config.bSupportsGLSync;
}
}  // Anonymous namespace

OGLTexture::OGLTexture(const TextureConfig& tex_config, std::string_view name)
    : AbstractTexture(tex_config), m_name(name)
{
  DEBUG_ASSERT_MSG(VIDEO, !tex_config.IsMultisampled() || tex_config.levels == 1,
                   "OpenGL does not support multisampled textures with mip levels");

  const GLenum target = GetGLTarget();
  glGenTextures(1, &m_texId);
  glActiveTexture(GL_MUTABLE_TEXTURE_INDEX);
  glBindTexture(target, m_texId);

  if (!m_name.empty() && g_ActiveConfig.backend_info.bSupportsSettingObjectNames)
  {
    glObjectLabel(GL_TEXTURE, m_texId, (GLsizei)m_name.size(), m_name.c_str());
  }

  glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, m_config.levels - 1);

  GLenum gl_internal_format = GetGLInternalFormatForTextureFormat(m_config.format, true);
  if (g_ogl_config.bSupportsTextureStorage && m_config.type == AbstractTextureType::Texture_CubeMap)
  {
    glTexStorage2D(target, m_config.levels, gl_internal_format, m_config.width, m_config.height);
  }
  else if (tex_config.IsMultisampled())
  {
    ASSERT(g_ogl_config.bSupportsMSAA);
    if (m_config.type == AbstractTextureType::Texture_2DArray)
    {
      if (g_ogl_config.SupportedMultisampleTexStorage != MultisampleTexStorageType::TexStorageNone)
      {
        glTexStorage3DMultisample(target, tex_config.samples, gl_internal_format, m_config.width,
                                  m_config.height, m_config.layers, GL_FALSE);
      }
      else
      {
        ASSERT(!g_ogl_config.bIsES);
        glTexImage3DMultisample(target, tex_config.samples, gl_internal_format, m_config.width,
                                m_config.height, m_config.layers, GL_FALSE);
      }
    }
    else if (m_config.type == AbstractTextureType::Texture_2D)
    {
      if (g_ogl_config.SupportedMultisampleTexStorage != MultisampleTexStorageType::TexStorageNone)
      {
        glTexStorage2DMultisample(target, tex_config.samples, gl_internal_format, m_config.width,
                                  m_config.height, GL_FALSE);
      }
      else
      {
        ASSERT(!g_ogl_config.bIsES);
        glTexImage2DMultisample(target, tex_config.samples, gl_internal_format, m_config.width,
                                m_config.height, GL_FALSE);
      }
    }
    else
    {
      ASSERT(false);
    }
  }
  else if (g_ogl_config.bSupportsTextureStorage)
  {
    if (m_config.type == AbstractTextureType::Texture_2DArray)
    {
      glTexStorage3D(target, m_config.levels, gl_internal_format, m_config.width, m_config.height,
                     m_config.layers);
    }
    else if (m_config.type == AbstractTextureType::Texture_2D)
    {
      glTexStorage2D(target, m_config.levels, gl_internal_format, m_config.width, m_config.height);
    }
    else
    {
      ASSERT(false);
    }
  }

  if (m_config.IsRenderTarget())
  {
    // We can't render to compressed formats.
    ASSERT(!IsCompressedFormat(m_config.format));
    if (!g_ogl_config.bSupportsTextureStorage && !tex_config.IsMultisampled())
    {
      for (u32 level = 0; level < m_config.levels; level++)
      {
        glTexImage3D(target, level, gl_internal_format, std::max(m_config.width >> level, 1u),
                     std::max(m_config.height >> level, 1u), m_config.layers, 0,
                     GetGLFormatForTextureFormat(m_config.format),
                     GetGLTypeForTextureFormat(m_config.format), nullptr);
      }
    }
  }
}

OGLTexture::~OGLTexture()
{
  GetOGLGfx()->UnbindTexture(this);
  glDeleteTextures(1, &m_texId);
}

void OGLTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                          const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                          u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                          u32 dst_layer, u32 dst_level)
{
  const OGLTexture* src_gltex = static_cast<const OGLTexture*>(src);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  if (g_ogl_config.bSupportsCopySubImage)
  {
    glCopyImageSubData(src_gltex->m_texId, src_gltex->GetGLTarget(), src_level, src_rect.left,
                       src_rect.top, src_layer, m_texId, GetGLTarget(), dst_level, dst_rect.left,
                       dst_rect.top, dst_layer, dst_rect.GetWidth(), dst_rect.GetHeight(), 1);
  }
  else
  {
    BlitFramebuffer(const_cast<OGLTexture*>(src_gltex), src_rect, src_layer, src_level, dst_rect,
                    dst_layer, dst_level);
  }
}

void OGLTexture::BlitFramebuffer(OGLTexture* srcentry, const MathUtil::Rectangle<int>& src_rect,
                                 u32 src_layer, u32 src_level,
                                 const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                 u32 dst_level)
{
  GetOGLGfx()->BindSharedReadFramebuffer();
  glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, srcentry->m_texId, src_level,
                            src_layer);
  GetOGLGfx()->BindSharedDrawFramebuffer();
  glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_texId, dst_level,
                            dst_layer);

  // glBlitFramebuffer is still affected by the scissor test, which is enabled by default.
  glDisable(GL_SCISSOR_TEST);

  glBlitFramebuffer(src_rect.left, src_rect.top, src_rect.right, src_rect.bottom, dst_rect.left,
                    dst_rect.top, dst_rect.right, dst_rect.bottom, GL_COLOR_BUFFER_BIT, GL_NEAREST);

  // The default state for the scissor test is enabled. We don't need to do a full state
  // restore, as the framebuffer and scissor test are the only things we changed.
  glEnable(GL_SCISSOR_TEST);
  GetOGLGfx()->RestoreFramebufferBinding();
}

void OGLTexture::ResolveFromTexture(const AbstractTexture* src,
                                    const MathUtil::Rectangle<int>& rect, u32 layer, u32 level)
{
  const OGLTexture* srcentry = static_cast<const OGLTexture*>(src);
  DEBUG_ASSERT(m_config.samples > 1 && m_config.width == srcentry->m_config.width &&
               m_config.height == srcentry->m_config.height && m_config.samples == 1);
  DEBUG_ASSERT(rect.left + rect.GetWidth() <= static_cast<int>(srcentry->m_config.width) &&
               rect.top + rect.GetHeight() <= static_cast<int>(srcentry->m_config.height));
  BlitFramebuffer(const_cast<OGLTexture*>(srcentry), rect, layer, level, rect, layer, level);
}

void OGLTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                      size_t buffer_size, u32 layer)
{
  if (level >= m_config.levels)
    PanicAlertFmt("Texture only has {} levels, can't update level {}", m_config.levels, level);

  if (layer >= m_config.layers)
    PanicAlertFmt("Texture only has {} layer, can't update layer {}", m_config.layers, layer);

  const auto expected_width = std::max(1U, m_config.width >> level);
  const auto expected_height = std::max(1U, m_config.height >> level);
  if (width != expected_width || height != expected_height)
  {
    PanicAlertFmt("Size of level {} must be {}x{}, but {}x{} requested", level, expected_width,
                  expected_height, width, height);
  }

  const GLenum target = GetGLTarget();
  glActiveTexture(GL_MUTABLE_TEXTURE_INDEX);
  glBindTexture(target, m_texId);

  if (row_length != width)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);

  GLenum gl_internal_format = GetGLInternalFormatForTextureFormat(m_config.format, false);
  if (IsCompressedFormat(m_config.format))
  {
    if (m_config.type == AbstractTextureType::Texture_CubeMap)
    {
      if (g_ogl_config.bSupportsTextureStorage)
      {
        glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, level, 0, 0, width,
                                  height, gl_internal_format, static_cast<GLsizei>(buffer_size),
                                  buffer);
      }
      else
      {
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, level, gl_internal_format,
                               width, height, 0, static_cast<GLsizei>(buffer_size), buffer);
      }
    }
    else if (m_config.type == AbstractTextureType::Texture_2D)
    {
      if (g_ogl_config.bSupportsTextureStorage)
      {
        glCompressedTexSubImage2D(target, level, 0, 0, width, height, gl_internal_format,
                                  static_cast<GLsizei>(buffer_size), buffer);
      }
      else
      {
        glCompressedTexImage2D(target, level, gl_internal_format, width, height, 0,
                               static_cast<GLsizei>(buffer_size), buffer);
      }
    }
    else if (m_config.type == AbstractTextureType::Texture_2DArray)
    {
      if (g_ogl_config.bSupportsTextureStorage)
      {
        glCompressedTexSubImage3D(target, level, 0, 0, layer, width, height, 1, gl_internal_format,
                                  static_cast<GLsizei>(buffer_size), buffer);
      }
      else
      {
        glCompressedTexImage3D(target, level, gl_internal_format, width, height, 1, 0,
                               static_cast<GLsizei>(buffer_size), buffer);
      }
    }
    else
    {
      PanicAlertFmt("Failed to handle compressed texture load - unhandled type");
    }
  }
  else
  {
    GLenum gl_format = GetGLFormatForTextureFormat(m_config.format);
    GLenum gl_type = GetGLTypeForTextureFormat(m_config.format);
    if (m_config.type == AbstractTextureType::Texture_CubeMap)
    {
      if (g_ogl_config.bSupportsTextureStorage)
      {
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, level, 0, 0, width, height,
                        gl_format, gl_type, buffer);
      }
      else
      {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, level, gl_internal_format, width,
                     height, 0, gl_format, gl_type, buffer);
      }
    }
    else if (m_config.type == AbstractTextureType::Texture_2D)
    {
      if (g_ogl_config.bSupportsTextureStorage)
      {
        glTexSubImage2D(target, level, 0, 0, width, height, gl_format, gl_type, buffer);
      }
      else
      {
        glTexImage2D(target, level, gl_internal_format, width, height, 0, gl_format, gl_type,
                     buffer);
      }
    }
    else if (m_config.type == AbstractTextureType::Texture_2DArray)
    {
      if (g_ogl_config.bSupportsTextureStorage)
      {
        glTexSubImage3D(target, level, 0, 0, layer, width, height, 1, gl_format, gl_type, buffer);
      }
      else
      {
        glTexImage3D(target, level, gl_internal_format, width, height, 1, 0, gl_format, gl_type,
                     buffer);
      }
    }
    else
    {
      PanicAlertFmt("Failed to handle texture load - unhandled type");
    }
  }

  if (row_length != width)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

GLenum OGLTexture::GetGLFormatForImageTexture() const
{
  return GetGLInternalFormatForTextureFormat(m_config.format, true);
}

OGLStagingTexture::OGLStagingTexture(StagingTextureType type, const TextureConfig& config,
                                     GLenum target, GLuint buffer_name, size_t buffer_size,
                                     char* map_ptr, size_t map_stride)
    : AbstractStagingTexture(type, config), m_target(target), m_buffer_name(buffer_name),
      m_buffer_size(buffer_size)
{
  m_map_pointer = map_ptr;
  m_map_stride = map_stride;
}

OGLStagingTexture::~OGLStagingTexture()
{
  if (m_fence != nullptr)
    glDeleteSync(m_fence);
  if (m_map_pointer)
  {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_buffer_name);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  }
  if (m_buffer_name != 0)
    glDeleteBuffers(1, &m_buffer_name);
}

std::unique_ptr<OGLStagingTexture> OGLStagingTexture::Create(StagingTextureType type,
                                                             const TextureConfig& config)
{
  size_t stride = config.GetStride();
  size_t buffer_size = stride * config.height;
  GLenum target =
      type == StagingTextureType::Readback ? GL_PIXEL_PACK_BUFFER : GL_PIXEL_UNPACK_BUFFER;
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(target, buffer);

  // Prefer using buffer_storage where possible. This allows us to skip the map/unmap steps.
  char* buffer_ptr;
  if (UsePersistentStagingBuffers())
  {
    GLenum buffer_flags;
    GLenum map_flags;
    if (type == StagingTextureType::Readback)
    {
      buffer_flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;
      map_flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;
    }
    else if (type == StagingTextureType::Upload)
    {
      buffer_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
      map_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
    }
    else
    {
      buffer_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
      map_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
    }

    glBufferStorage(target, buffer_size, nullptr, buffer_flags);
    buffer_ptr = static_cast<char*>(glMapBufferRange(target, 0, buffer_size, map_flags));
    ASSERT(buffer_ptr != nullptr);
  }
  else
  {
    // Otherwise, fallback to mapping the buffer each time.
    glBufferData(target, buffer_size, nullptr,
                 type == StagingTextureType::Readback ? GL_STREAM_READ : GL_STREAM_DRAW);
    buffer_ptr = nullptr;
  }
  glBindBuffer(target, 0);

  return std::unique_ptr<OGLStagingTexture>(
      new OGLStagingTexture(type, config, target, buffer, buffer_size, buffer_ptr, stride));
}

void OGLStagingTexture::CopyFromTexture(const AbstractTexture* src,
                                        const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                        u32 src_level, const MathUtil::Rectangle<int>& dst_rect)
{
  ASSERT(m_type == StagingTextureType::Readback || m_type == StagingTextureType::Mutable);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= src->GetConfig().width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= src->GetConfig().height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= m_config.width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= m_config.height);

  // Unmap the buffer before writing when not using persistent mappings.
  if (!UsePersistentStagingBuffers())
    OGLStagingTexture::Unmap();

  // Copy from the texture object to the staging buffer.
  glBindBuffer(GL_PIXEL_PACK_BUFFER, m_buffer_name);
  glPixelStorei(GL_PACK_ROW_LENGTH, m_config.width);

  const OGLTexture* gltex = static_cast<const OGLTexture*>(src);
  const size_t dst_offset = dst_rect.top * m_config.GetStride() + dst_rect.left * m_texel_size;

  // Prefer glGetTextureSubImage(), when available.
  if (g_ogl_config.bSupportsTextureSubImage)
  {
    glGetTextureSubImage(
        gltex->GetGLTextureId(), src_level, src_rect.left, src_rect.top, src_layer,
        src_rect.GetWidth(), src_rect.GetHeight(), 1, GetGLFormatForTextureFormat(src->GetFormat()),
        GetGLTypeForTextureFormat(src->GetFormat()),
        static_cast<GLsizei>(m_buffer_size - dst_offset), reinterpret_cast<void*>(dst_offset));
  }
  else
  {
    // Mutate the shared framebuffer.
    GetOGLGfx()->BindSharedReadFramebuffer();
    if (AbstractTexture::IsDepthFormat(gltex->GetFormat()))
    {
      glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0);
      glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, gltex->GetGLTextureId(),
                                src_level, src_layer);
    }
    else
    {
      glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gltex->GetGLTextureId(),
                                src_level, src_layer);
      glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0, 0);
    }
    glReadPixels(src_rect.left, src_rect.top, src_rect.GetWidth(), src_rect.GetHeight(),
                 GetGLFormatForTextureFormat(src->GetFormat()),
                 GetGLTypeForTextureFormat(src->GetFormat()), reinterpret_cast<void*>(dst_offset));
    GetOGLGfx()->RestoreFramebufferBinding();
  }

  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

  // If we support buffer storage, create a fence for synchronization.
  if (UsePersistentStagingBuffers())
  {
    if (m_fence != nullptr)
      glDeleteSync(m_fence);

    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    m_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
  }

  m_needs_flush = true;
}

void OGLStagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect,
                                      AbstractTexture* dst,
                                      const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                      u32 dst_level)
{
  ASSERT(m_type == StagingTextureType::Upload || m_type == StagingTextureType::Mutable);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= m_config.width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= m_config.height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= dst->GetConfig().width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= dst->GetConfig().height);

  const OGLTexture* gltex = static_cast<const OGLTexture*>(dst);
  const size_t src_offset = src_rect.top * m_config.GetStride() + src_rect.left * m_texel_size;
  const size_t copy_size = src_rect.GetHeight() * m_config.GetStride();

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_buffer_name);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, m_config.width);

  if (!UsePersistentStagingBuffers())
  {
    // Unmap the buffer before writing when not using persistent mappings.
    if (m_map_pointer)
    {
      glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
      m_map_pointer = nullptr;
    }
  }
  else
  {
    // Since we're not using coherent mapping, we must flush the range explicitly.
    if (m_type == StagingTextureType::Upload)
      glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, src_offset, copy_size);
    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
  }

  // Copy from the staging buffer to the texture object.
  const GLenum target = gltex->GetGLTarget();
  glActiveTexture(GL_MUTABLE_TEXTURE_INDEX);
  glBindTexture(target, gltex->GetGLTextureId());
  glTexSubImage3D(target, 0, dst_rect.left, dst_rect.top, dst_layer, dst_rect.GetWidth(),
                  dst_rect.GetHeight(), 1, GetGLFormatForTextureFormat(dst->GetFormat()),
                  GetGLTypeForTextureFormat(dst->GetFormat()), reinterpret_cast<void*>(src_offset));

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  // If we support buffer storage, create a fence for synchronization.
  if (UsePersistentStagingBuffers())
  {
    if (m_fence != nullptr)
      glDeleteSync(m_fence);

    m_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
  }

  m_needs_flush = true;
}

void OGLStagingTexture::Flush()
{
  // No-op when not using buffer storage, as the transfers happen on Map().
  // m_fence will always be zero in this case.
  if (m_fence == nullptr)
  {
    m_needs_flush = false;
    return;
  }

  glClientWaitSync(m_fence, 0, GL_TIMEOUT_IGNORED);
  glDeleteSync(m_fence);
  m_fence = nullptr;
  m_needs_flush = false;
}

bool OGLStagingTexture::Map()
{
  if (m_map_pointer)
    return true;

  // Slow path, map the texture, unmap it later.
  GLenum flags;
  if (m_type == StagingTextureType::Readback)
    flags = GL_MAP_READ_BIT;
  else if (m_type == StagingTextureType::Upload)
    flags = GL_MAP_WRITE_BIT;
  else
    flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
  glBindBuffer(m_target, m_buffer_name);
  m_map_pointer = static_cast<char*>(glMapBufferRange(m_target, 0, m_buffer_size, flags));
  glBindBuffer(m_target, 0);
  return m_map_pointer != nullptr;
}

void OGLStagingTexture::Unmap()
{
  // No-op with persistent mapped buffers.
  if (!m_map_pointer || UsePersistentStagingBuffers())
    return;

  glBindBuffer(m_target, m_buffer_name);
  glUnmapBuffer(m_target);
  glBindBuffer(m_target, 0);
  m_map_pointer = nullptr;
}

OGLFramebuffer::OGLFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                               std::vector<AbstractTexture*> additional_color_attachments,
                               AbstractTextureFormat color_format,
                               AbstractTextureFormat depth_format, u32 width, u32 height,
                               u32 layers, u32 samples, GLuint fbo)
    : AbstractFramebuffer(color_attachment, depth_attachment,
                          std::move(additional_color_attachments), color_format, depth_format,
                          width, height, layers, samples),
      m_fbo(fbo)
{
}

OGLFramebuffer::~OGLFramebuffer()
{
  glDeleteFramebuffers(1, &m_fbo);
}

std::unique_ptr<OGLFramebuffer>
OGLFramebuffer::Create(OGLTexture* color_attachment, OGLTexture* depth_attachment,
                       std::vector<AbstractTexture*> additional_color_attachments)
{
  if (!ValidateConfig(color_attachment, depth_attachment, additional_color_attachments))
    return nullptr;

  const AbstractTextureFormat color_format =
      color_attachment ? color_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const AbstractTextureFormat depth_format =
      depth_attachment ? depth_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const OGLTexture* either_attachment = color_attachment ? color_attachment : depth_attachment;
  const u32 width = either_attachment->GetWidth();
  const u32 height = either_attachment->GetHeight();
  const u32 layers = either_attachment->GetLayers();
  const u32 samples = either_attachment->GetSamples();

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  std::vector<GLenum> buffers;
  if (color_attachment)
  {
    if (color_attachment->GetConfig().layers > 1)
    {
      glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_attachment->GetGLTextureId(),
                           0);
    }
    else
    {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                color_attachment->GetGLTextureId(), 0, 0);
    }
    buffers.push_back(GL_COLOR_ATTACHMENT0);
  }

  if (depth_attachment)
  {
    GLenum attachment = AbstractTexture::IsStencilFormat(depth_format) ?
                            GL_DEPTH_STENCIL_ATTACHMENT :
                            GL_DEPTH_ATTACHMENT;
    if (depth_attachment->GetConfig().layers > 1)
    {
      glFramebufferTexture(GL_FRAMEBUFFER, attachment, depth_attachment->GetGLTextureId(), 0);
    }
    else
    {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, depth_attachment->GetGLTextureId(), 0,
                                0);
    }
  }

  for (std::size_t i = 0; i < additional_color_attachments.size(); i++)
  {
    const auto attachment_enum = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i + 1);
    OGLTexture* attachment = static_cast<OGLTexture*>(additional_color_attachments[i]);
    if (attachment->GetConfig().layers > 1)
    {
      glFramebufferTexture(GL_FRAMEBUFFER, attachment_enum, attachment->GetGLTextureId(), 0);
    }
    else
    {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment_enum, attachment->GetGLTextureId(), 0,
                                0);
    }
    buffers.push_back(attachment_enum);
  }

  glDrawBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());

  DEBUG_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  GetOGLGfx()->RestoreFramebufferBinding();

  return std::make_unique<OGLFramebuffer>(color_attachment, depth_attachment,
                                          std::move(additional_color_attachments), color_format,
                                          depth_format, width, height, layers, samples, fbo);
}

void OGLFramebuffer::UpdateDimensions(u32 width, u32 height)
{
  m_width = width;
  m_height = height;
}

}  // namespace OGL
