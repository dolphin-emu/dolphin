// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/GL/GLContext.h"

#if defined(__APPLE__)
#include "Common/GL/GLInterface/AGL.h"
#elif defined(_WIN32)
#include "Common/GL/GLInterface/WGL.h"
#elif HAVE_X11
#if defined(USE_EGL) && USE_EGL
#include "Common/GL/GLInterface/EGLX11.h"
#else
#include "Common/GL/GLInterface/GLX.h"
#endif
#elif defined(USE_EGL) && USE_EGL && defined(USE_HEADLESS)
#include "Common/GL/GLInterface/EGL.h"
#elif ANDROID
#include "Common/GL/GLInterface/EGLAndroid.h"
#error Platform doesnt have a GLInterface
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

std::unique_ptr<GLContext> GLContext::Create(const WindowSystemInfo& wsi, bool stereo, bool core)
{
  std::unique_ptr<GLContext> context;
#if defined(__APPLE__)
  context = std::make_unique<GLContextAGL>();
#elif defined(_WIN32)
  context = std::make_unique<GLContextWGL>();
#elif defined(USE_EGL) && defined(USE_HEADLESS)
  context = std::make_unique<GLContextEGL>();
#elif defined(HAVE_X11) && HAVE_X11
#if defined(USE_EGL) && USE_EGL
  context = std::make_unique<GLContextEGLX11>();
#else
  context = std::make_unique<GLContextGLX>();
#endif
#elif ANDROID
  context = std::make_unique<GLContextEGLAndroid>();
#else
  return nullptr;
#endif
  if (!context->Initialize(wsi.display_connection, wsi.render_surface, stereo, core))
    return nullptr;

  return context;
}
