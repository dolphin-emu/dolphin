// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/EGLWayland.h"

#include "Common/Logging/Log.h"

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif
#ifndef EGL_PLATFORM_WAYLAND_EXT
#define EGL_PLATFORM_WAYLAND_EXT 0x31D8
#endif

EGLDisplay GLContextEGLWayland::OpenEGLDisplay()
{
  if (!m_wsi.display_connection)
  {
    ERROR_LOG_FMT(VIDEO, "GLContextEGLWayland: missing wl_display in WindowSystemInfo");
    return EGL_NO_DISPLAY;
  }

  auto get_platform_display = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
      eglGetProcAddress("eglGetPlatformDisplayEXT"));
  if (get_platform_display)
  {
    EGLDisplay display =
        get_platform_display(EGL_PLATFORM_WAYLAND_EXT, m_wsi.display_connection, nullptr);
    if (display != EGL_NO_DISPLAY)
      return display;
  }

  return eglGetDisplay(static_cast<EGLNativeDisplayType>(m_wsi.display_connection));
}

EGLNativeWindowType GLContextEGLWayland::GetEGLNativeWindow(EGLConfig config)
{
  if (!m_wsi.render_surface)
  {
    ERROR_LOG_FMT(VIDEO, "GLContextEGLWayland: missing wl_egl_window in WindowSystemInfo");
    return static_cast<EGLNativeWindowType>(0);
  }

  return reinterpret_cast<EGLNativeWindowType>(m_wsi.render_surface);
}
