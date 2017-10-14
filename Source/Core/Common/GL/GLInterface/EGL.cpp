// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "Common/GL/GLInterface/EGL.h"
#include "Common/Logging/Log.h"
#include "Core/Config/GraphicsSettings.h"

#ifndef EGL_KHR_create_context
#define EGL_KHR_create_context 1
#define EGL_CONTEXT_MAJOR_VERSION_KHR 0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#define EGL_CONTEXT_FLAGS_KHR 0x30FC
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR 0x31BD
#define EGL_NO_RESET_NOTIFICATION_KHR 0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_KHR 0x31BF
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR 0x00000004
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR 0x00000002
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif /* EGL_KHR_create_context */

// Show the current FPS
void cInterfaceEGL::Swap()
{
  if (egl_surf != EGL_NO_SURFACE)
    eglSwapBuffers(egl_dpy, egl_surf);
}
void cInterfaceEGL::SwapInterval(int Interval)
{
  eglSwapInterval(egl_dpy, Interval);
}

void* cInterfaceEGL::GetFuncAddress(const std::string& name)
{
  return (void*)eglGetProcAddress(name.c_str());
}

void cInterfaceEGL::DetectMode()
{
  if (s_opengl_mode != GLInterfaceMode::MODE_DETECT)
    return;
  bool preferGLES = Config::Get(Config::GFX_PREFER_GLES);

  EGLint num_configs;
  bool supportsGL = false, supportsGLES2 = false, supportsGLES3 = false;
  std::array<int, 3> renderable_types{{
      EGL_OPENGL_BIT, (1 << 6), /* EGL_OPENGL_ES3_BIT_KHR */
      EGL_OPENGL_ES2_BIT,
  }};

  for (auto renderable_type : renderable_types)
  {
    // attributes for a visual in RGBA format with at least
    // 8 bits per color
    int attribs[] = {EGL_RED_SIZE,  8, EGL_GREEN_SIZE,      8,
                     EGL_BLUE_SIZE, 8, EGL_RENDERABLE_TYPE, renderable_type,
                     EGL_NONE};

    // Get how many configs there are
    if (!eglChooseConfig(egl_dpy, attribs, nullptr, 0, &num_configs))
    {
      INFO_LOG(VIDEO, "Error: couldn't get an EGL visual config");
      continue;
    }

    EGLConfig* config = new EGLConfig[num_configs];

    // Get all the configurations
    if (!eglChooseConfig(egl_dpy, attribs, config, num_configs, &num_configs))
    {
      INFO_LOG(VIDEO, "Error: couldn't get an EGL visual config");
      delete[] config;
      continue;
    }

    for (int i = 0; i < num_configs; ++i)
    {
      EGLint attribVal;
      bool ret;
      ret = eglGetConfigAttrib(egl_dpy, config[i], EGL_RENDERABLE_TYPE, &attribVal);
      if (ret)
      {
        if (attribVal & EGL_OPENGL_BIT)
          supportsGL = true;
        if (attribVal & (1 << 6)) /* EGL_OPENGL_ES3_BIT_KHR */
          supportsGLES3 = true;
        if (attribVal & EGL_OPENGL_ES2_BIT)
          supportsGLES2 = true;
      }
    }
    delete[] config;
  }

  if (preferGLES)
  {
    if (supportsGLES3)
      s_opengl_mode = GLInterfaceMode::MODE_OPENGLES3;
    else if (supportsGLES2)
      s_opengl_mode = GLInterfaceMode::MODE_OPENGLES2;
    else if (supportsGL)
      s_opengl_mode = GLInterfaceMode::MODE_OPENGL;
  }
  else
  {
    if (supportsGL)
      s_opengl_mode = GLInterfaceMode::MODE_OPENGL;
    else if (supportsGLES3)
      s_opengl_mode = GLInterfaceMode::MODE_OPENGLES3;
    else if (supportsGLES2)
      s_opengl_mode = GLInterfaceMode::MODE_OPENGLES2;
  }

  if (s_opengl_mode == GLInterfaceMode::MODE_OPENGL)
  {
    INFO_LOG(VIDEO, "Using OpenGL");
  }
  else if (s_opengl_mode == GLInterfaceMode::MODE_OPENGLES3)
  {
    INFO_LOG(VIDEO, "Using OpenGL|ES 3");
  }
  else if (s_opengl_mode == GLInterfaceMode::MODE_OPENGLES2)
  {
    INFO_LOG(VIDEO, "Using OpenGL|ES 2");
  }
  else if (s_opengl_mode == GLInterfaceMode::MODE_DETECT)
  {
    // Errored before we found a mode
    ERROR_LOG(VIDEO, "Error: Failed to detect OpenGL flavour, falling back to OpenGL");
    // This will fail to create a context, as it'll try to use the same attribs we just failed to
    // find a matching config with
    s_opengl_mode = GLInterfaceMode::MODE_OPENGL;
  }
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceEGL::Create(void* window_handle, bool stereo, bool core)
{
  EGLint egl_major, egl_minor;
  bool supports_core_profile = false;

  egl_dpy = OpenDisplay();
  m_host_window = (EGLNativeWindowType)window_handle;
  m_has_handle = !!window_handle;
  m_core = core;

  if (!egl_dpy)
  {
    INFO_LOG(VIDEO, "Error: eglGetDisplay() failed");
    return false;
  }

  if (!eglInitialize(egl_dpy, &egl_major, &egl_minor))
  {
    INFO_LOG(VIDEO, "Error: eglInitialize() failed");
    return false;
  }

  /* Detection code */
  EGLint num_configs;

  DetectMode();

  // attributes for a visual in RGBA format with at least
  // 8 bits per color
  int attribs[] = {EGL_RENDERABLE_TYPE,
                   EGL_OPENGL_ES2_BIT,
                   EGL_RED_SIZE,
                   8,
                   EGL_GREEN_SIZE,
                   8,
                   EGL_BLUE_SIZE,
                   8,
                   EGL_NONE};

  std::vector<EGLint> ctx_attribs;
  switch (s_opengl_mode)
  {
  case GLInterfaceMode::MODE_OPENGL:
    attribs[1] = EGL_OPENGL_BIT;
    ctx_attribs = {EGL_NONE};
    break;
  case GLInterfaceMode::MODE_OPENGLES2:
    attribs[1] = EGL_OPENGL_ES2_BIT;
    ctx_attribs = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    break;
  case GLInterfaceMode::MODE_OPENGLES3:
    attribs[1] = (1 << 6); /* EGL_OPENGL_ES3_BIT_KHR */
    ctx_attribs = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    break;
  default:
    ERROR_LOG(VIDEO, "Unknown opengl mode set");
    return false;
    break;
  }

  if (!eglChooseConfig(egl_dpy, attribs, &m_config, 1, &num_configs))
  {
    INFO_LOG(VIDEO, "Error: couldn't get an EGL visual config");
    return false;
  }

  if (s_opengl_mode == GLInterfaceMode::MODE_OPENGL)
    eglBindAPI(EGL_OPENGL_API);
  else
    eglBindAPI(EGL_OPENGL_ES_API);

  std::string tmp;
  std::istringstream buffer(eglQueryString(egl_dpy, EGL_EXTENSIONS));
  while (buffer >> tmp)
  {
    if (tmp == "EGL_KHR_surfaceless_context")
      m_supports_surfaceless = true;
    else if (tmp == "EGL_KHR_create_context")
      supports_core_profile = true;
  }

  if (supports_core_profile && core && s_opengl_mode == GLInterfaceMode::MODE_OPENGL)
  {
    std::array<std::pair<int, int>, 7> versions_to_try = {{
        {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3},
    }};

    for (const auto& version : versions_to_try)
    {
      std::vector<EGLint> core_attribs = {EGL_CONTEXT_MAJOR_VERSION_KHR,
                                          version.first,
                                          EGL_CONTEXT_MINOR_VERSION_KHR,
                                          version.second,
                                          EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                          EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                                          EGL_CONTEXT_FLAGS_KHR,
                                          EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
                                          EGL_NONE};

      egl_ctx = eglCreateContext(egl_dpy, m_config, EGL_NO_CONTEXT, &core_attribs[0]);
      if (egl_ctx)
      {
        m_attribs = std::move(core_attribs);
        break;
      }
    }
  }

  if (!egl_ctx)
  {
    m_core = false;
    egl_ctx = eglCreateContext(egl_dpy, m_config, EGL_NO_CONTEXT, &ctx_attribs[0]);
    m_attribs = std::move(ctx_attribs);
  }

  if (!egl_ctx)
  {
    INFO_LOG(VIDEO, "Error: eglCreateContext failed");
    return false;
  }

  if (!CreateWindowSurface())
  {
    ERROR_LOG(VIDEO, "Error: CreateWindowSurface failed 0x%04x", eglGetError());
    return false;
  }
  return true;
}

std::unique_ptr<cInterfaceBase> cInterfaceEGL::CreateSharedContext()
{
  std::unique_ptr<cInterfaceBase> context = std::make_unique<cInterfaceEGL>();
  if (!context->Create(this))
    return nullptr;
  return context;
}

bool cInterfaceEGL::Create(cInterfaceBase* main_context)
{
  cInterfaceEGL* egl_context = static_cast<cInterfaceEGL*>(main_context);

  egl_dpy = egl_context->egl_dpy;
  m_core = egl_context->m_core;
  m_config = egl_context->m_config;
  m_supports_surfaceless = egl_context->m_supports_surfaceless;
  m_is_shared = true;
  m_has_handle = false;

  if (egl_context->GetMode() == GLInterfaceMode::MODE_OPENGL)
    eglBindAPI(EGL_OPENGL_API);
  else
    eglBindAPI(EGL_OPENGL_ES_API);

  egl_ctx =
      eglCreateContext(egl_dpy, m_config, egl_context->egl_ctx, egl_context->m_attribs.data());
  if (!egl_ctx)
  {
    INFO_LOG(VIDEO, "Error: eglCreateContext failed 0x%04x", eglGetError());
    return false;
  }

  if (!CreateWindowSurface())
  {
    ERROR_LOG(VIDEO, "Error: CreateWindowSurface failed 0x%04x", eglGetError());
    return false;
  }
  return true;
}

bool cInterfaceEGL::CreateWindowSurface()
{
  if (m_has_handle)
  {
    EGLNativeWindowType native_window = InitializePlatform(m_host_window, m_config);
    egl_surf = eglCreateWindowSurface(egl_dpy, m_config, native_window, nullptr);
    if (!egl_surf)
    {
      INFO_LOG(VIDEO, "Error: eglCreateWindowSurface failed");
      return false;
    }
  }
  else if (!m_supports_surfaceless)
  {
    EGLint attrib_list[] = {
        EGL_NONE,
    };
    egl_surf = eglCreatePbufferSurface(egl_dpy, m_config, attrib_list);
    if (!egl_surf)
    {
      INFO_LOG(VIDEO, "Error: eglCreatePbufferSurface failed");
      return false;
    }
  }
  else
  {
    egl_surf = EGL_NO_SURFACE;
  }
  return true;
}

void cInterfaceEGL::DestroyWindowSurface()
{
  if (egl_surf != EGL_NO_SURFACE && !eglDestroySurface(egl_dpy, egl_surf))
    NOTICE_LOG(VIDEO, "Could not destroy window surface.");
  egl_surf = EGL_NO_SURFACE;
}

bool cInterfaceEGL::MakeCurrent()
{
  return eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx);
}

void cInterfaceEGL::UpdateHandle(void* window_handle)
{
  m_host_window = (EGLNativeWindowType)window_handle;
  m_has_handle = !!window_handle;
}

void cInterfaceEGL::UpdateSurface()
{
  ClearCurrent();
  DestroyWindowSurface();
  CreateWindowSurface();
  MakeCurrent();
}

bool cInterfaceEGL::ClearCurrent()
{
  return eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

// Close backend
void cInterfaceEGL::Shutdown()
{
  ShutdownPlatform();
  if (egl_ctx)
  {
    if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx))
      NOTICE_LOG(VIDEO, "Could not release drawing context.");
    eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (!eglDestroyContext(egl_dpy, egl_ctx))
      NOTICE_LOG(VIDEO, "Could not destroy drawing context.");
    DestroyWindowSurface();
    if (!m_is_shared && !eglTerminate(egl_dpy))
      NOTICE_LOG(VIDEO, "Could not destroy display connection.");
    egl_ctx = nullptr;
  }
}
