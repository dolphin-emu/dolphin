// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/GL/GLInterfaceBase.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/OGLTexture.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConfig.h"

namespace OGL
{
namespace
{
std::array<u32, 8> s_Textures;
u32 s_ActiveTexture;

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
  default:
    return storage ? GL_RGBA8 : GL_RGBA;
  }
}

GLenum GetGLFormatForTextureFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::RGBA8:
    return GL_RGBA;
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
    return GL_UNSIGNED_BYTE;
  // Compressed texture formats don't use this parameter.
  default:
    return GL_UNSIGNED_BYTE;
  }
}
}  // Anonymous namespace

bool SaveTexture(const std::string& filename, u32 textarget, u32 tex, int virtual_width,
                 int virtual_height, unsigned int level)
{
  if (GLInterface->GetMode() != GLInterfaceMode::MODE_OPENGL)
    return false;
  int width = std::max(virtual_width >> level, 1);
  int height = std::max(virtual_height >> level, 1);
  std::vector<u8> data(width * height * 4);
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(textarget, tex);
  glGetTexImage(textarget, level, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
  OGLTexture::SetStage();

  return TextureToPng(data.data(), width * 4, filename, width, height, true);
}

OGLTexture::OGLTexture(const TextureConfig& tex_config) : AbstractTexture(tex_config)
{
  glGenTextures(1, &m_texId);

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_texId);

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, m_config.levels - 1);

  if (g_ogl_config.bSupportsTextureStorage)
  {
    GLenum gl_internal_format = GetGLInternalFormatForTextureFormat(m_config.format, true);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, m_config.levels, gl_internal_format, m_config.width,
                   m_config.height, m_config.layers);
  }

  if (m_config.rendertarget)
  {
    // We can't render to compressed formats.
    _assert_(!IsCompressedHostTextureFormat(m_config.format));

    if (!g_ogl_config.bSupportsTextureStorage)
    {
      for (u32 level = 0; level < m_config.levels; level++)
      {
        glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, std::max(m_config.width >> level, 1u),
                     std::max(m_config.height >> level, 1u), m_config.layers, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
      }
    }
    glGenFramebuffers(1, &m_framebuffer);
    FramebufferManager::SetFramebuffer(m_framebuffer);
    FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D_ARRAY, m_texId, 0);
  }

  SetStage();
}

OGLTexture::~OGLTexture()
{
  if (m_texId)
  {
    for (auto& gtex : s_Textures)
      if (gtex == m_texId)
        gtex = 0;
    glDeleteTextures(1, &m_texId);
    m_texId = 0;
  }

  if (m_framebuffer)
  {
    glDeleteFramebuffers(1, &m_framebuffer);
    m_framebuffer = 0;
  }
}

GLuint OGLTexture::GetRawTexIdentifier() const
{
  return m_texId;
}

GLuint OGLTexture::GetFramebuffer() const
{
  return m_framebuffer;
}

void OGLTexture::Bind(unsigned int stage)
{
  if (s_Textures[stage] != m_texId)
  {
    if (s_ActiveTexture != stage)
    {
      glActiveTexture(GL_TEXTURE0 + stage);
      s_ActiveTexture = stage;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_texId);
    s_Textures[stage] = m_texId;
  }
}

bool OGLTexture::Save(const std::string& filename, unsigned int level)
{
  // We can't dump compressed textures currently (it would mean drawing them to a RGBA8
  // framebuffer, and saving that). TextureCache does not call Save for custom textures
  // anyway, so this is fine for now.
  _assert_(m_config.format == AbstractTextureFormat::RGBA8);

  return SaveTexture(filename, GL_TEXTURE_2D_ARRAY, m_texId, m_config.width, m_config.height,
                     level);
}

void OGLTexture::CopyRectangleFromTexture(const AbstractTexture* source,
                                          const MathUtil::Rectangle<int>& srcrect,
                                          const MathUtil::Rectangle<int>& dstrect)
{
  const OGLTexture* srcentry = static_cast<const OGLTexture*>(source);
  if (srcrect.GetWidth() == dstrect.GetWidth() && srcrect.GetHeight() == dstrect.GetHeight() &&
      g_ogl_config.bSupportsCopySubImage)
  {
    glCopyImageSubData(srcentry->m_texId, GL_TEXTURE_2D_ARRAY, 0, srcrect.left, srcrect.top, 0,
                       m_texId, GL_TEXTURE_2D_ARRAY, 0, dstrect.left, dstrect.top, 0,
                       dstrect.GetWidth(), dstrect.GetHeight(), srcentry->m_config.layers);
    return;
  }
  else if (!m_framebuffer)
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
  FramebufferManager::SetFramebuffer(0);
  g_renderer->RestoreAPIState();
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
  if (IsCompressedHostTextureFormat(m_config.format))
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

  SetStage();
}

void OGLTexture::DisableStage(unsigned int stage)
{
}

void OGLTexture::SetStage()
{
  // -1 is the initial value as we don't know which texture should be bound
  if (s_ActiveTexture != (u32)-1)
    glActiveTexture(GL_TEXTURE0 + s_ActiveTexture);
}

}  // namespace OGL
