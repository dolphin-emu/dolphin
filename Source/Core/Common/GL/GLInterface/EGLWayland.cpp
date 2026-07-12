// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/EGLWayland.h"

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

GLContextEGLWayland::~GLContextEGLWayland()
{
  if (m_window && m_wl_egl_window_destroy)
    m_wl_egl_window_destroy(m_window);
}

EGLDisplay GLContextEGLWayland::OpenEGLDisplay()
{
  return eglGetDisplay(static_cast<EGLNativeDisplayType>(m_wsi.display_connection));
}

EGLNativeWindowType GLContextEGLWayland::GetEGLNativeWindow(EGLConfig config)
{
  if (!m_wayland_lib.IsOpen())
  {
    const std::string name = Common::DynamicLibrary::GetVersionedFilename("wayland-egl", 1);
    if (!m_wayland_lib.Open(name.c_str()))
    {
      PanicAlertFmt("Failed to load {}", name);
      return EGLNativeWindowType{};
    }
#define LOAD(x)                                                                                    \
  if (!m_wayland_lib.GetSymbol(#x, &m_##x))                                                        \
  {                                                                                                \
    PanicAlertFmt("Failed to load symbol " #x);                                                    \
    return EGLNativeWindowType{};                                                                  \
  }
    LOAD(wl_egl_window_create);
    LOAD(wl_egl_window_destroy);
#undef LOAD
  }

  if (m_window)
  {
    m_wl_egl_window_destroy(m_window);
    m_window = nullptr;
  }

  const int width = m_wsi.render_surface_width > 0 ? m_wsi.render_surface_width : 640;
  const int height = m_wsi.render_surface_height > 0 ? m_wsi.render_surface_height : 480;
  m_window = m_wl_egl_window_create(static_cast<wl_surface*>(m_wsi.render_surface), width, height);
  if (!m_window)
  {
    ERROR_LOG_FMT(VIDEO, "GLContextEGLWayland: wl_egl_window_create failed");
    return EGLNativeWindowType{};
  }

  m_backbuffer_width = static_cast<u32>(width);
  m_backbuffer_height = static_cast<u32>(height);
  return reinterpret_cast<EGLNativeWindowType>(m_window);
}
