// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/EGLWayland.h"

#include <algorithm>

#include "Common/MsgHandler.h"

GLContextEGLWayland::~GLContextEGLWayland()
{
  if (m_window)
    m_wl_egl_window_destroy(m_window);
}

void GLContextEGLWayland::SetSurfaceSize(u32 width, u32 height)
{
  m_wsi.render_surface_width = width;
  m_wsi.render_surface_height = height;
}

void GLContextEGLWayland::Update()
{
  m_backbuffer_width = std::max(m_wsi.render_surface_width, 1);
  m_backbuffer_height = std::max(m_wsi.render_surface_height, 1);
  if (m_window)
    m_wl_egl_window_resize(m_window, m_backbuffer_width, m_backbuffer_height, 0, 0);
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
                                    std::max(m_wsi.render_surface_width, 1),
                                    std::max(m_wsi.render_surface_height, 1));
  return reinterpret_cast<EGLNativeWindowType>(m_window);
}
