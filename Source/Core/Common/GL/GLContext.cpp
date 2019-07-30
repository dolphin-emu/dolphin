// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/GL/GLContext.h"

#if defined(__APPLE__)
#include "Common/GL/GLInterface/AGL.h"
#endif
#if defined(WIN32)
#include "Common/GL/GLInterface/WGL.h"
#endif
#if HAVE_X11
#include "Common/GL/GLInterface/GLX.h"
#endif
#if HAVE_EGL
#include "Common/GL/GLInterface/EGL.h"
#if HAVE_X11
#include "Common/GL/GLInterface/EGLX11.h"
#endif
#if defined(ANDROID)
#include "Common/GL/GLInterface/EGLAndroid.h"
#endif
#endif

const std::array<std::pair<int, int>, 9> GLContext::s_desktop_opengl_versions = {
    {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2}}};

GLContext::~GLContext() = default;

std::unique_ptr<GLContext> GLContext::Create(const WindowSystemInfo& wsi, bool core,
                                             bool prefer_egl, bool prefer_gles)
{
  std::unique_ptr<GLContext> context;
#if defined(__APPLE__)
  if (wsi.type == WindowSystemType::MacOS || wsi.type == WindowSystemType::Headless)
    context = std::make_unique<GLContextAGL>();
#endif
#if defined(_WIN32)
  if (wsi.type == WindowSystemType::Windows)
    context = std::make_unique<GLContextWGL>();
#endif
#if defined(ANDROID)
  if (wsi.type == WindowSystemType::Android)
    context = std::make_unique<GLContextEGLAndroid>();
#endif
#if HAVE_X11
  if (wsi.type == WindowSystemType::X11)
  {
    // GLES 3 is not supported via GLX.
    const bool use_egl = prefer_egl || prefer_gles;
#if defined(HAVE_EGL)
    if (use_egl)
      context = std::make_unique<GLContextEGLX11>();
    else
      context = std::make_unique<GLContextGLX>();
#else
    context = std::make_unique<GLContextGLX>();
#endif
  }
#endif
#if HAVE_EGL
  if (wsi.type == WindowSystemType::Headless)
    context = std::make_unique<GLContextEGL>();
#endif

  if (!context)
    return nullptr;

  // Option to prefer GLES on desktop platforms, useful for testing.
  if (prefer_gles)
    context->m_opengl_mode = Mode::OpenGLES;

  if (!context->Initialize(wsi.display_connection, wsi.render_surface, core))
    return nullptr;

  return context;
}
