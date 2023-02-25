// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/SWOGLWindow.h"

#include <memory>

#include "Common/GL/GLContext.h"
#include "Common/GL/GLUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Software/SWTexture.h"

SWOGLWindow::SWOGLWindow() = default;
SWOGLWindow::~SWOGLWindow() = default;

std::unique_ptr<SWOGLWindow> SWOGLWindow::Create(const WindowSystemInfo& wsi)
{
  std::unique_ptr<SWOGLWindow> window = std::unique_ptr<SWOGLWindow>(new SWOGLWindow());
  if (!window->Initialize(wsi))
  {
    PanicAlertFmt("Failed to create OpenGL window");
    return nullptr;
  }

  return window;
}

bool SWOGLWindow::IsHeadless() const
{
  return m_gl_context->IsHeadless();
}

bool SWOGLWindow::Initialize(const WindowSystemInfo& wsi)
{
  m_gl_context = GLContext::Create(wsi);
  if (!m_gl_context)
    return false;

  // Init extension support.
  if (!GLExtensions::Init(m_gl_context.get()))
  {
    ERROR_LOG_FMT(VIDEO, "GLExtensions::Init failed!Does your video card support OpenGL 2.0?");
    return false;
  }
  else if (GLExtensions::Version() < 310)
  {
    ERROR_LOG_FMT(VIDEO, "OpenGL Version {} detected, but at least 3.1 is required.",
                  GLExtensions::Version());
    return false;
  }

  std::string frag_shader = "in vec2 TexCoord;\n"
                            "out vec4 ColorOut;\n"
                            "uniform sampler2D samp;\n"
                            "void main() {\n"
                            "	ColorOut = texture(samp, TexCoord);\n"
                            "}\n";

  std::string vertex_shader = "out vec2 TexCoord;\n"
                              "void main() {\n"
                              "	vec2 rawpos = vec2(gl_VertexID & 1, (gl_VertexID & 2) >> 1);\n"
                              "	gl_Position = vec4(rawpos * 2.0 - 1.0, 0.0, 1.0);\n"
                              "	TexCoord = vec2(rawpos.x, -rawpos.y);\n"
                              "}\n";

  std::string header = m_gl_context->IsGLES() ? "#version 300 es\n"
                                                "precision highp float;\n" :
                                                "#version 140\n";

  m_image_program = GLUtil::CompileProgram(header + vertex_shader, header + frag_shader);

  glUseProgram(m_image_program);

  glUniform1i(glGetUniformLocation(m_image_program, "samp"), 0);
  glGenTextures(1, &m_image_texture);
  glBindTexture(GL_TEXTURE_2D, m_image_texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glGenVertexArrays(1, &m_image_vao);
  return true;
}

void SWOGLWindow::ShowImage(const AbstractTexture* image,
                            const MathUtil::Rectangle<int>& xfb_region)
{
  const SW::SWTexture* sw_image = static_cast<const SW::SWTexture*>(image);
  m_gl_context->Update();  // just updates the render window position and the backbuffer size

  GLsizei glWidth = (GLsizei)m_gl_context->GetBackBufferWidth();
  GLsizei glHeight = (GLsizei)m_gl_context->GetBackBufferHeight();

  glViewport(0, 0, glWidth, glHeight);

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D, m_image_texture);

  // TODO: Apply xfb_region

  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment
  glPixelStorei(GL_UNPACK_ROW_LENGTH, sw_image->GetConfig().width);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(sw_image->GetConfig().width),
               static_cast<GLsizei>(sw_image->GetConfig().height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
               sw_image->GetData(0, 0));

  glUseProgram(m_image_program);

  glBindVertexArray(m_image_vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  m_gl_context->Swap();
}
