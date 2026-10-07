// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/EGLWayland.h"
#include "Common/MsgHandler.h"

GLContextEGLWayland::~GLContextEGLWayland()
{
  if (m_window)
    m_wl_egl_window_destroy(m_window);
}

void GLContextEGLWayland::Update(u32 width, u32 height)
{
  GLContext::Update(width, height);
  if (m_window)
    m_wl_egl_window_resize(m_window, width, height, 0, 0);
}

EGLDisplay GLContextEGLWayland::OpenEGLDisplay()
{
  return eglGetDisplay(static_cast<EGLNativeDisplayType>(m_wsi.display_connection));
}

EGLNativeWindowType GLContextEGLWayland::GetEGLNativeWindow(EGLConfig config)
{
  if (!m_wayland_lib.IsOpen())
  {
    std::string name = Common::DynamicLibrary::GetVersionedFilename("wayland-egl", 1);
    m_wayland_lib.Open(name.c_str());
    if (!m_wayland_lib.IsOpen())
    {
      PanicAlertFmt("Failed to load {}", name);
      return reinterpret_cast<EGLNativeWindowType>(m_window);
    }
#define LOAD(x) m_wayland_lib.GetSymbol(#x, &m_##x)
    LOAD(wl_egl_window_create);
    LOAD(wl_egl_window_destroy);
    LOAD(wl_egl_window_resize);
#undef LOAD
  }
  if (m_window)
    m_wl_egl_window_destroy(m_window);
  m_window = m_wl_egl_window_create(static_cast<wl_surface*>(m_wsi.render_surface),
                                    m_wsi.render_surface_width, m_wsi.render_surface_height);
  return reinterpret_cast<EGLNativeWindowType>(m_window);
}
