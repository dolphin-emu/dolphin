// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterfaceBase.h"

#include "VideoBackends/OGL/AbstractTexture.h"
#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"

#include "VideoCommon/ImageWrite.h"

namespace OGL
{
GLint AbstractTexture::s_ActiveTexture;
std::array<GLint, 16> AbstractTexture::s_Textures;

void AbstractTexture::SetStage()
{
  // -1 is the initial value as we don't know which texture should be bound
  if (s_ActiveTexture != (u32)-1)
    glActiveTexture(GL_TEXTURE0 + s_ActiveTexture);
}

AbstractTexture::AbstractTexture(const TextureConfig& config) : AbstractTextureBase(config)
{
  glGenTextures(1, &texture);

  framebuffer = 0;

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, config.levels - 1);

  if (config.rendertarget)
  {
    for (u32 level = 0; level <= config.levels; level++)
    {
      glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, config.width, config.height, config.layers,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    glGenFramebuffers(1, &framebuffer);
    FramebufferManager::SetFramebuffer(framebuffer);
    FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D_ARRAY, texture, 0);
  }

  SetStage();
}

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
  AbstractTexture::SetStage();

  return TextureToPng(data.data(), width * 4, filename, width, height, true);
}

AbstractTexture::~AbstractTexture()
{
  if (texture)
  {
    for (auto& gtex : s_Textures)
      if (gtex == texture)
        gtex = 0;
    glDeleteTextures(1, &texture);
    texture = 0;
  }

  if (framebuffer)
  {
    glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0;
  }
}

void AbstractTexture::Bind(unsigned int stage)
{
  if (s_Textures[stage] != texture)
  {
    if (s_ActiveTexture != stage)
    {
      glActiveTexture(GL_TEXTURE0 + stage);
      s_ActiveTexture = stage;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    s_Textures[stage] = texture;
  }
}

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  return SaveTexture(filename, GL_TEXTURE_2D_ARRAY, texture, config.width, config.height, level);
}

void AbstractTexture::CopyRectangleFromTexture(const AbstractTextureBase* source,
                                               const MathUtil::Rectangle<int>& srcrect,
                                               const MathUtil::Rectangle<int>& dstrect)
{
  const AbstractTexture* srcentry = static_cast<const AbstractTexture*>(source);
  if (srcrect.GetWidth() == dstrect.GetWidth() && srcrect.GetHeight() == dstrect.GetHeight() &&
      g_ogl_config.bSupportsCopySubImage)
  {
    glCopyImageSubData(srcentry->texture, GL_TEXTURE_2D_ARRAY, 0, srcrect.left, srcrect.top, 0,
                       texture, GL_TEXTURE_2D_ARRAY, 0, dstrect.left, dstrect.top, 0,
                       dstrect.GetWidth(), dstrect.GetHeight(), srcentry->config.layers);
    return;
  }
  else if (!framebuffer)
  {
    glGenFramebuffers(1, &framebuffer);
    FramebufferManager::SetFramebuffer(framebuffer);
    FramebufferManager::FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D_ARRAY, texture, 0);
  }
  g_renderer->ResetAPIState();
  FramebufferManager::SetFramebuffer(framebuffer);
  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, srcentry->texture);
  g_sampler_cache->BindLinearSampler(9);
  glViewport(dstrect.left, dstrect.top, dstrect.GetWidth(), dstrect.GetHeight());
  s_ColorCopyProgram.Bind();
  glUniform4f(s_ColorCopyPositionUniform, float(srcrect.left), float(srcrect.top),
              float(srcrect.GetWidth()), float(srcrect.GetHeight()));
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  FramebufferManager::SetFramebuffer(0);
  g_renderer->RestoreAPIState();
}

void AbstractTexture::Load(u8* data, unsigned int width, unsigned int height,
                           unsigned int expanded_width, unsigned int level)
{
  if (level >= config.levels)
    PanicAlert("Texture only has %d levels, can't update level %d", config.levels, level);
  if (width != std::max(1u, config.width >> level) ||
      height != std::max(1u, config.height >> level))
    PanicAlert("size of level %d must be %dx%d, but %dx%d requested", level,
               std::max(1u, config.width >> level), std::max(1u, config.height >> level), width,
               height);

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

  if (expanded_width != width)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);

  glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, width, height, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               data);

  if (expanded_width != width)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  SetStage();
}

void AbstractTexture::FromRenderTarget(u8* dstPointer, PEControl::PixelFormat srcFormat,
                                       const EFBRectangle& srcRect, bool scaleByHalf,
                                       unsigned int cbufid, const float* colmat)
{
  g_renderer->ResetAPIState();  // reset any game specific settings

  // Make sure to resolve anything we need to read from.
  const GLuint read_texture = (srcFormat == PEControl::Z24) ?
                                  FramebufferManager::ResolveAndGetDepthTarget(srcRect) :
                                  FramebufferManager::ResolveAndGetRenderTarget(srcRect);

  FramebufferManager::SetFramebuffer(framebuffer);

  OpenGL_BindAttributelessVAO();

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, read_texture);
  if (scaleByHalf)
    g_sampler_cache->BindLinearSampler(9);
  else
    g_sampler_cache->BindNearestSampler(9);

  glViewport(0, 0, config.width, config.height);

  GLuint uniform_location;
  if (srcFormat == PEControl::Z24)
  {
    s_DepthMatrixProgram.Bind();
    if (s_DepthCbufid != cbufid)
      glUniform4fv(s_DepthMatrixUniform, 5, colmat);
    s_DepthCbufid = cbufid;
    uniform_location = s_DepthCopyPositionUniform;
  }
  else
  {
    s_ColorMatrixProgram.Bind();
    if (s_ColorCbufid != cbufid)
      glUniform4fv(s_ColorMatrixUniform, 7, colmat);
    s_ColorCbufid = cbufid;
    uniform_location = s_ColorMatrixPositionUniform;
  }

  TargetRectangle R = g_renderer->ConvertEFBRectangle(srcRect);
  glUniform4f(uniform_location, static_cast<float>(R.left), static_cast<float>(R.top),
              static_cast<float>(R.right), static_cast<float>(R.bottom));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  FramebufferManager::SetFramebuffer(0);
  g_renderer->RestoreAPIState();
}
}