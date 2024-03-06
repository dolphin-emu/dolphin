// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLUtil.h"

#include <memory>

#include "Common/Assert.h"
#include "Common/GL/GLContext.h"
#include "Common/Logging/Log.h"

namespace GLUtil
{
GLuint CompileProgram(const std::string& vertexShader, const std::string& fragmentShader)
{
  // generate objects
  GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
  GLuint programID = glCreateProgram();

  // compile vertex shader
  const char* shader = vertexShader.c_str();
  glShaderSource(vertexShaderID, 1, &shader, nullptr);
  glCompileShader(vertexShaderID);
#if defined(_DEBUG) || defined(DEBUGFAST)
  GLint Result = GL_FALSE;
  char stringBuffer[1024];
  GLsizei stringBufferUsage = 0;
  glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderInfoLog(vertexShaderID, 1024, &stringBufferUsage, stringBuffer);

  if (Result && stringBufferUsage)
  {
    ERROR_LOG_FMT(VIDEO, "GLSL vertex shader warnings:\n{}{}", stringBuffer, vertexShader);
  }
  else if (!Result)
  {
    ERROR_LOG_FMT(VIDEO, "GLSL vertex shader error:\n{}{}", stringBuffer, vertexShader);
  }
  else
  {
    INFO_LOG_FMT(VIDEO, "GLSL vertex shader compiled:\n{}", vertexShader);
  }

  bool shader_errors = !Result;
#endif

  // compile fragment shader
  shader = fragmentShader.c_str();
  glShaderSource(fragmentShaderID, 1, &shader, nullptr);
  glCompileShader(fragmentShaderID);
#if defined(_DEBUG) || defined(DEBUGFAST)
  glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderInfoLog(fragmentShaderID, 1024, &stringBufferUsage, stringBuffer);

  if (Result && stringBufferUsage)
  {
    ERROR_LOG_FMT(VIDEO, "GLSL fragment shader warnings:\n{}{}", stringBuffer, fragmentShader);
  }
  else if (!Result)
  {
    ERROR_LOG_FMT(VIDEO, "GLSL fragment shader error:\n{}{}", stringBuffer, fragmentShader);
  }
  else
  {
    INFO_LOG_FMT(VIDEO, "GLSL fragment shader compiled:\n{}", fragmentShader);
  }

  shader_errors |= !Result;
#endif

  // link them
  glAttachShader(programID, vertexShaderID);
  glAttachShader(programID, fragmentShaderID);
  glLinkProgram(programID);
#if defined(_DEBUG) || defined(DEBUGFAST)
  glGetProgramiv(programID, GL_LINK_STATUS, &Result);
  glGetProgramInfoLog(programID, 1024, &stringBufferUsage, stringBuffer);

  if (Result && stringBufferUsage)
  {
    ERROR_LOG_FMT(VIDEO, "GLSL linker warnings:\n{}{}{}", stringBuffer, vertexShader,
                  fragmentShader);
  }
  else if (!Result && !shader_errors)
  {
    ERROR_LOG_FMT(VIDEO, "GLSL linker error:\n{}{}{}", stringBuffer, vertexShader, fragmentShader);
  }
#endif

  // cleanup
  glDeleteShader(vertexShaderID);
  glDeleteShader(fragmentShaderID);

  return programID;
}

void EnablePrimitiveRestart(const GLContext* context)
{
  constexpr GLuint PRIMITIVE_RESTART_INDEX = 65535;

  if (context->IsGLES())
  {
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
  }
  else
  {
    if (GLExtensions::Version() >= 310)
    {
      glEnable(GL_PRIMITIVE_RESTART);
      glPrimitiveRestartIndex(PRIMITIVE_RESTART_INDEX);
    }
    else
    {
      glEnableClientState(GL_PRIMITIVE_RESTART_NV);
      glPrimitiveRestartIndexNV(PRIMITIVE_RESTART_INDEX);
    }
  }
}

void BindFrameBuffer(GLuint buffer)
{
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer);
}

void ClearViewportRect(int x, int y, int width, int height)
{
  glEnable(GL_SCISSOR_TEST);
  glViewport(x, y, width, height);
  glScissor(x, y, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);
}

bool CreateFrameBuffer(GLuint& buffer, GLuint color, GLuint depth, GLuint stencil)
{
  glGenFramebuffers(1, &buffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencil, 0);
  GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE)
  {
    ERROR_LOG_FMT(VIDEO, "Incomplete frame buffer object: {}", status);
    return false;
  }
  return true;
}

void DestroyFrameBuffers(GLuint* buffers, int count)
{
  glDeleteFramebuffers(count, buffers);
  delete[] buffers;
}
}  // namespace GLUtil
