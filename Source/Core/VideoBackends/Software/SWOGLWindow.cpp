// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/GL/GLInterfaceBase.h"
#include "Common/GL/GLUtil.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoCommon/AbstractTexture.h"

std::unique_ptr<SWOGLWindow> SWOGLWindow::s_instance;

void SWOGLWindow::Init(void* window_handle)
{
  InitInterface();
  GLInterface->SetMode(GLInterfaceMode::MODE_DETECT);
  if (!GLInterface->Create(window_handle))
  {
    ERROR_LOG(VIDEO, "GLInterface::Create failed.");
  }

  s_instance.reset(new SWOGLWindow());
}

void SWOGLWindow::Shutdown()
{
  GLInterface->Shutdown();
  GLInterface.reset();

  s_instance.reset();
}

void SWOGLWindow::Prepare()
{
  if (m_init)
    return;
  m_init = true;

  // Init extension support.
  if (!GLExtensions::Init())
  {
    ERROR_LOG(VIDEO, "GLExtensions::Init failed!Does your video card support OpenGL 2.0?");
    return;
  }
  else if (GLExtensions::Version() < 310)
  {
    ERROR_LOG(VIDEO, "OpenGL Version %d detected, but at least 3.1 is required.",
              GLExtensions::Version());
    return;
  }

  std::string frag_shader = "in vec2 TexCoord;\n"
                            "out vec4 ColorOut;\n"
                            "uniform sampler2DArray samp;\n"
                            "void main() {\n"
                            "	ColorOut = texture(samp, vec3(TexCoord, 0.0));\n"
                            "}\n";

  std::string vertex_shader = "out vec2 TexCoord;\n"
                              "void main() {\n"
                              "	vec2 rawpos = vec2(gl_VertexID & 1, (gl_VertexID & 2) >> 1);\n"
                              "	gl_Position = vec4(rawpos * 2.0 - 1.0, 0.0, 1.0);\n"
                              "	TexCoord = vec2(rawpos.x, -rawpos.y);\n"
                              "}\n";

  std::string header = GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGL ?
                           "#version 140\n" :
                           "#version 300 es\n"
                           "precision highp float;\n";

  m_image_program = OpenGL_CompileProgram(header + vertex_shader, header + frag_shader);

  glUseProgram(m_image_program);

  glUniform1i(glGetUniformLocation(m_image_program, "samp"), 0);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment

  glGenVertexArrays(1, &m_image_vao);
}

void SWOGLWindow::PrintText(const std::string& text, int x, int y, u32 color)
{
  m_text.push_back({text, x, y, color});
}

void SWOGLWindow::ShowImage(AbstractTexture* image, float aspect)
{
  GLInterface->Update();  // just updates the render window position and the backbuffer size

  GLsizei glWidth = (GLsizei)GLInterface->GetBackBufferWidth();
  GLsizei glHeight = (GLsizei)GLInterface->GetBackBufferHeight();

  glViewport(0, 0, glWidth, glHeight);

  image->Bind(0);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glUseProgram(m_image_program);

  glBindVertexArray(m_image_vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // TODO: implement OSD
  //	for (TextData& text : m_text)
  //	{
  //	}
  m_text.clear();

  GLInterface->Swap();
}

int SWOGLWindow::PeekMessages()
{
  return GLInterface->PeekMessages();
}
