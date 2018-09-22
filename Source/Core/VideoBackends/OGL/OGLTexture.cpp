// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/OGLTexture.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConfig.h"

namespace OGL
{
namespace
{
GLenum GetGLInternalFormatForTextureFormat(AbstractTextureFormat format, bool storage)
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
    PanicAlert("Unhandled texture format.");
    return storage ? GL_RGBA8 : GL_RGBA;
  }
}

GLenum GetGLFormatForTextureFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::RGBA8:
    return GL_RGBA;
  case AbstractTextureFormat::BGRA8:
    return GL_BGRA;
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

OGLTexture::OGLTexture(const TextureConfig& tex_config) : AbstractTexture(tex_config)
{
  DEBUG_ASSERT_MSG(VIDEO, !tex_config.IsMultisampled() || tex_config.levels == 1,
                   "OpenGL does not support multisampled textures with mip levels");

  GLenum target =
      tex_config.IsMultisampled() ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
  glGenTextures(1, &m_texId);
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(target, m_texId);

  glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, m_config.levels - 1);

  GLenum gl_internal_format = GetGLInternalFormatForTextureFormat(m_config.format, true);
  if (tex_config.IsMultisampled())
  {
    if (g_ogl_config.bSupportsTextureStorage)
      glTexStorage3DMultisample(target, tex_config.samples, gl_internal_format, m_config.width,
                                m_config.height, m_config.layers, GL_FALSE);
    else
      glTexImage3DMultisample(target, tex_config.samples, gl_internal_format, m_config.width,
                              m_config.height, m_config.layers, GL_FALSE);
  }
  else if (g_ogl_config.bSupportsTextureStorage)
  {
    glTexStorage3D(target, m_config.levels, gl_internal_format, m_config.width, m_config.height,
                   m_config.layers);
  }

  if (m_config.rendertarget)
  {
    // We can't render to compressed formats.
    ASSERT(!IsCompressedFormat(m_config.format));
    if (!g_ogl_config.bSupportsTextureStorage && !tex_config.IsMultisampled())
    {
      for (u32 level = 0; level < m_config.levels; level++)
      {
        glTexImage3D(target, level, GL_RGBA, std::max(m_config.width >> level, 1u),
                     std::max(m_config.height >> level, 1u), m_config.layers, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
      }
    }
    glGenFramebuffers(1, &m_framebuffer);
    FramebufferManager::SetFramebuffer(m_framebuffer);
    FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, m_texId,
                                           0);

    // We broke the framebuffer binding here, and need to restore it, as the CreateTexture
    // method is in the base renderer class and can be called by VideoCommon.
    FramebufferManager::SetFramebuffer(0);
  }
}

OGLTexture::~OGLTexture()
{
  g_renderer->UnbindTexture(this);
  if (m_texId)
    glDeleteTextures(1, &m_texId);

  if (m_framebuffer)
    glDeleteFramebuffers(1, &m_framebuffer);
}

GLuint OGLTexture::GetRawTexIdentifier() const
{
  return m_texId;
}

GLuint OGLTexture::GetFramebuffer() const
{
  return m_framebuffer;
}

void OGLTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                          const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                          u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                          u32 dst_layer, u32 dst_level)
{
  const OGLTexture* srcentry = static_cast<const OGLTexture*>(src);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  if (g_ogl_config.bSupportsCopySubImage)
  {
    glCopyImageSubData(srcentry->m_texId, GL_TEXTURE_2D_ARRAY, src_level, src_rect.left,
                       src_rect.top, src_layer, m_texId, GL_TEXTURE_2D_ARRAY, dst_level,
                       dst_rect.left, dst_rect.top, dst_layer, dst_rect.GetWidth(),
                       dst_rect.GetHeight(), 1);
  }
  else
  {
    BlitFramebuffer(const_cast<OGLTexture*>(srcentry), src_rect, src_layer, src_level, dst_rect,
                    dst_layer, dst_level);
  }
}

void OGLTexture::BlitFramebuffer(OGLTexture* srcentry, const MathUtil::Rectangle<int>& src_rect,
                                 u32 src_layer, u32 src_level,
                                 const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                 u32 dst_level)
{
  // If it isn't a single leveled/layered texture, we need to update the framebuffer.
  bool update_src_framebuffer =
      srcentry->m_framebuffer == 0 || srcentry->m_config.layers != 0 || src_level != 0;
  bool update_dst_framebuffer = m_framebuffer == 0 || m_config.layers != 0 || dst_level != 0;
  if (!m_framebuffer)
    glGenFramebuffers(1, &m_framebuffer);
  if (!srcentry->m_framebuffer)
    glGenFramebuffers(1, &const_cast<OGLTexture*>(srcentry)->m_framebuffer);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, srcentry->m_framebuffer);
  if (update_src_framebuffer)
  {
    glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, srcentry->m_texId,
                              src_level, src_layer);
  }

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_framebuffer);
  if (update_dst_framebuffer)
  {
    glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_texId, dst_level,
                              dst_layer);
  }

  // glBlitFramebuffer is still affected by the scissor test, which is enabled by default.
  glDisable(GL_SCISSOR_TEST);

  glBlitFramebuffer(src_rect.left, src_rect.top, src_rect.right, src_rect.bottom, dst_rect.left,
                    dst_rect.top, dst_rect.right, dst_rect.bottom, GL_COLOR_BUFFER_BIT, GL_NEAREST);

  if (update_src_framebuffer)
  {
    FramebufferManager::FramebufferTexture(
        GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        srcentry->m_config.IsMultisampled() ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY,
        srcentry->m_texId, 0);
  }
  if (update_dst_framebuffer)
  {
    FramebufferManager::FramebufferTexture(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        m_config.IsMultisampled() ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY, m_texId,
        0);
  }

  // The default state for the scissor test is enabled. We don't need to do a full state
  // restore, as the framebuffer and scissor test are the only things we changed.
  glEnable(GL_SCISSOR_TEST);
  FramebufferManager::SetFramebuffer(0);
}

void OGLTexture::ScaleRectangleFromTexture(const AbstractTexture* source,
                                           const MathUtil::Rectangle<int>& srcrect,
                                           const MathUtil::Rectangle<int>& dstrect)
{
  const OGLTexture* srcentry = static_cast<const OGLTexture*>(source);
  if (!m_framebuffer)
  {
    glGenFramebuffers(1, &m_framebuffer);
    FramebufferManager::SetFramebuffer(m_framebuffer);
    FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D_ARRAY, m_texId, 0);
  }
  g_renderer->ResetAPIState();
  FramebufferManager::SetFramebuffer(m_framebuffer);
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, srcentry->m_texId);
  g_sampler_cache->BindLinearSampler(9);
  glViewport(dstrect.left, dstrect.top, dstrect.GetWidth(), dstrect.GetHeight());
  TextureCache::GetInstance()->GetColorCopyProgram().Bind();
  glUniform4f(TextureCache::GetInstance()->GetColorCopyPositionUniform(), float(srcrect.left),
              float(srcrect.top), float(srcrect.GetWidth()), float(srcrect.GetHeight()));
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  g_renderer->RestoreAPIState();
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
                      size_t buffer_size)
{
  if (level >= m_config.levels)
    PanicAlert("Texture only has %d levels, can't update level %d", m_config.levels, level);
  if (width != std::max(1u, m_config.width >> level) ||
      height != std::max(1u, m_config.height >> level))
    PanicAlert("size of level %d must be %dx%d, but %dx%d requested", level,
               std::max(1u, m_config.width >> level), std::max(1u, m_config.height >> level), width,
               height);

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_texId);

  if (row_length != width)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);

  GLenum gl_internal_format = GetGLInternalFormatForTextureFormat(m_config.format, false);
  if (IsCompressedFormat(m_config.format))
  {
    if (g_ogl_config.bSupportsTextureStorage)
    {
      glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, 0, width, height, 1,
                                gl_internal_format, static_cast<GLsizei>(buffer_size), buffer);
    }
    else
    {
      glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_internal_format, width, height, 1, 0,
                             static_cast<GLsizei>(buffer_size), buffer);
    }
  }
  else
  {
    GLenum gl_format = GetGLFormatForTextureFormat(m_config.format);
    GLenum gl_type = GetGLTypeForTextureFormat(m_config.format);
    if (g_ogl_config.bSupportsTextureStorage)
    {
      glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, 0, width, height, 1, gl_format, gl_type,
                      buffer);
    }
    else
    {
      glTexImage3D(GL_TEXTURE_2D_ARRAY, level, gl_internal_format, width, height, 1, 0, gl_format,
                   gl_type, buffer);
    }
  }

  if (row_length != width)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
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
  if (m_fence != 0)
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
    buffer_ptr =
        reinterpret_cast<char*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, buffer_size, map_flags));
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
  ASSERT(m_type == StagingTextureType::Readback);
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
  size_t dst_offset = dst_rect.top * m_config.GetStride() + dst_rect.left * m_texel_size;

  // If we don't have a FBO associated with this texture, we need to use a slow path.
  if (gltex->GetFramebuffer() != 0 && src_layer == 0 && src_level == 0)
  {
    // This texture has a framebuffer, so we can use glReadPixels().
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gltex->GetFramebuffer());
    glReadPixels(src_rect.left, src_rect.top, src_rect.GetWidth(), src_rect.GetHeight(),
                 GetGLFormatForTextureFormat(m_config.format),
                 GetGLTypeForTextureFormat(m_config.format), reinterpret_cast<void*>(dst_offset));

    // Reset both read/draw framebuffers.
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferManager::GetEFBFramebuffer());
  }
  else
  {
    if (g_ogl_config.bSupportsTextureSubImage)
    {
      glGetTextureSubImage(
          gltex->GetRawTexIdentifier(), src_level, src_rect.left, src_rect.top, src_layer,
          src_rect.GetWidth(), src_rect.GetHeight(), 1,
          GetGLFormatForTextureFormat(m_config.format), GetGLTypeForTextureFormat(m_config.format),
          static_cast<GLsizei>(m_buffer_size - dst_offset), reinterpret_cast<void*>(dst_offset));
    }
    else
    {
      // TODO: Investigate whether it's faster to use glReadPixels() with a framebuffer, since we're
      // copying the whole texture, which may waste bandwidth. So we're trading CPU work in creating
      // the framebuffer for GPU work in copying potentially redundant texels.
      glActiveTexture(GL_TEXTURE9);
      glBindTexture(GL_TEXTURE_2D_ARRAY, gltex->GetRawTexIdentifier());
      glGetTexImage(GL_TEXTURE_2D_ARRAY, src_level, GetGLFormatForTextureFormat(m_config.format),
                    GetGLTypeForTextureFormat(m_config.format), nullptr);
    }
  }

  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

  // If we support buffer storage, create a fence for synchronization.
  if (UsePersistentStagingBuffers())
  {
    if (m_fence != 0)
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
  ASSERT(m_type == StagingTextureType::Upload);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= m_config.width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= m_config.height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= dst->GetConfig().width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= dst->GetConfig().height);

  size_t src_offset = src_rect.top * m_config.GetStride() + src_rect.left * m_texel_size;
  size_t copy_size = src_rect.GetHeight() * m_config.GetStride();

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
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<const OGLTexture*>(dst)->GetRawTexIdentifier());
  glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, dst_rect.left, dst_rect.top, dst_layer,
                  dst_rect.GetWidth(), dst_rect.GetHeight(), 1,
                  GetGLFormatForTextureFormat(m_config.format),
                  GetGLTypeForTextureFormat(m_config.format), reinterpret_cast<void*>(src_offset));

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  // If we support buffer storage, create a fence for synchronization.
  if (UsePersistentStagingBuffers())
  {
    if (m_fence != 0)
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
  if (m_fence == 0)
  {
    m_needs_flush = false;
    return;
  }

  glClientWaitSync(m_fence, 0, GL_TIMEOUT_IGNORED);
  glDeleteSync(m_fence);
  m_fence = 0;
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
  m_map_pointer = reinterpret_cast<char*>(glMapBufferRange(m_target, 0, m_buffer_size, flags));
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

OGLFramebuffer::OGLFramebuffer(AbstractTextureFormat color_format,
                               AbstractTextureFormat depth_format, u32 width, u32 height,
                               u32 layers, u32 samples, GLuint fbo)
    : AbstractFramebuffer(color_format, depth_format, width, height, layers, samples), m_fbo(fbo)
{
}

OGLFramebuffer::~OGLFramebuffer()
{
  glDeleteFramebuffers(1, &m_fbo);
}

std::unique_ptr<OGLFramebuffer> OGLFramebuffer::Create(const OGLTexture* color_attachment,
                                                       const OGLTexture* depth_attachment)
{
  if (!ValidateConfig(color_attachment, depth_attachment))
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

  if (color_attachment)
  {
    if (color_attachment->GetConfig().layers > 1)
    {
      glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           color_attachment->GetRawTexIdentifier(), 0);
    }
    else
    {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                color_attachment->GetRawTexIdentifier(), 0, 0);
    }
  }

  if (depth_attachment)
  {
    GLenum attachment = AbstractTexture::IsStencilFormat(depth_format) ?
                            GL_DEPTH_STENCIL_ATTACHMENT :
                            GL_DEPTH_ATTACHMENT;
    if (depth_attachment->GetConfig().layers > 1)
    {
      glFramebufferTexture(GL_FRAMEBUFFER, attachment, depth_attachment->GetRawTexIdentifier(), 0);
    }
    else
    {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, depth_attachment->GetRawTexIdentifier(),
                                0, 0);
    }
  }

  DEBUG_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  FramebufferManager::SetFramebuffer(0);
  return std::make_unique<OGLFramebuffer>(color_format, depth_format, width, height, layers,
                                          samples, fbo);
}

}  // namespace OGL
