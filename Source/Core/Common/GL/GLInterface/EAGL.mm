// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <dlfcn.h>

#include "Common/GL/GLExtensions/GLExtensions.h"
#include "Common/GL/GLInterface/EAGL.h"
#include "Common/Logging/Log.h"

GLContextEAGL::~GLContextEAGL()
{
  if ([EAGLContext currentContext] == m_context)
    [EAGLContext setCurrentContext:nil];

  m_context = nil;
}

bool GLContextEAGL::IsHeadless() const
{
  return !m_layer;
}

void GLContextEAGL::Swap()
{
  glBindRenderbuffer(GL_RENDERBUFFER, m_color_renderbuffer);
  [m_context presentRenderbuffer:GL_RENDERBUFFER];
}

void* GLContextEAGL::GetFuncAddress(const std::string& name)
{
  return dlsym(RTLD_DEFAULT, name.c_str());
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool GLContextEAGL::Initialize(const WindowSystemInfo& wsi, bool stereo, bool core)
{
  m_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  [EAGLContext setCurrentContext:m_context];
    
  m_layer = (__bridge CAEAGLLayer*)wsi.render_surface;

  CGRect bounds = [m_layer bounds];
  m_backbuffer_width = (u32)CGRectGetWidth(bounds);
  m_backbuffer_height = (u32)CGRectGetHeight(bounds);

  m_opengl_mode = Mode::OpenGLES;

  return true;
}

GLuint GLContextEAGL::GetDefaultFramebuffer()
{
  if (m_framebuffer_created)
  {
    // TODO
  }

  glGenFramebuffers(1, &m_framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

  glGenRenderbuffers(1, &m_color_renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, m_color_renderbuffer);
  [m_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:m_layer];
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                            m_color_renderbuffer);

  GLint width;
  GLint height;
  glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
  glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE)
  {
    ERROR_LOG(VIDEO, "EAGL framebuffer not complete: 0x%x", status);
  }

  m_framebuffer_created = true;

  return m_framebuffer;
}

std::unique_ptr<GLContext> GLContextEAGL::CreateSharedContext()
{
  EAGLContext* new_eagl_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3
                                                        sharegroup:[m_context sharegroup]];
  if (new_eagl_context == nil)
  {
    ERROR_LOG(VIDEO, "failed to create shared context");
    return nullptr;
  }

  std::unique_ptr<GLContextEAGL> new_context = std::make_unique<GLContextEAGL>();
  new_context->m_layer = this->m_layer;
  new_context->m_context = new_eagl_context;
  new_context->m_is_shared = true;
  new_context->m_framebuffer_created = this->m_framebuffer_created;
  new_context->m_color_renderbuffer = this->m_color_renderbuffer;
  return new_context;
}

bool GLContextEAGL::MakeCurrent()
{
  [EAGLContext setCurrentContext:m_context];
  return true;
}

bool GLContextEAGL::ClearCurrent()
{
  [EAGLContext setCurrentContext:nil];
  return true;
}

void GLContextEAGL::Update()
{
  if (!m_layer)
    return;

  // TODO
  // CreateFramebuffer();
}

void GLContextEAGL::SwapInterval(int interval)
{
}
