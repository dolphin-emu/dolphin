// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/GL/GLContext.h"

#if defined(__APPLE__)
#include "Common/GL/GLInterface/AGL.h"
#elif defined(_WIN32)
#include "Common/GL/GLInterface/WGL.h"
#elif defined(ANDROID)
#include "Common/GL/GLInterface/EGLAndroid.h"
#elif HAVE_X11
#include "Common/GL/GLInterface/EGLX11.h"
#include "Common/GL/GLInterface/GLX.h"
#elif HAVE_EGL
#include "Common/GL/GLInterface/EGL.h"
#endif

GLContext::~GLContext() = default;

bool GLContext::Initialize(void* display_handle, void* window_handle, bool stereo, bool core)
{
  return false;
}

bool GLContext::IsHeadless() const
{
  return true;
}

std::unique_ptr<GLContext> GLContext::CreateSharedContext()
{
  return nullptr;
}

bool GLContext::MakeCurrent()
{
  return false;
}

bool GLContext::ClearCurrent()
{
  return false;
}

void GLContext::Update()
{
}

void GLContext::UpdateSurface(void* window_handle)
{
}

void GLContext::Swap()
{
}

void GLContext::SwapInterval(int interval)
{
}

void* GLContext::GetFuncAddress(const std::string& name)
{
  return nullptr;
}

std::unique_ptr<GLContext> GLContext::Create(const WindowSystemInfo& wsi, bool stereo, bool core,
                                             bool prefer_egl, bool prefer_gles)
{
  std::unique_ptr<GLContext> context;
#if defined(__APPLE__)
  context = std::make_unique<GLContextAGL>();
#elif defined(_WIN32)
  context = std::make_unique<GLContextWGL>();
#elif defined(ANDROID)
  context = std::make_unique<GLContextEGLAndroid>();
#elif HAVE_X11
  // GLES is not supported via GLX?
  if (prefer_egl || prefer_gles)
    context = std::make_unique<GLContextEGLX11>();
  else
    context = std::make_unique<GLContextGLX>();
#elif HAVE_EGL
  context = std::make_unique<GLContextEGL>();
#else
  return nullptr;
#endif

  // Option to prefer GLES on desktop platforms, useful for testing.
  if (prefer_gles)
    context->m_opengl_mode = Mode::OpenGLES;

  if (!context->Initialize(wsi.display_connection, wsi.render_surface, stereo, core))
    return nullptr;

  return context;
}
