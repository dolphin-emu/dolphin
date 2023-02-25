// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/EGLX11.h"

GLContextEGLX11::~GLContextEGLX11()
{
  // The context must be destroyed before the window.
  DestroyWindowSurface();
  DestroyContext();
  m_render_window.reset();
}

void GLContextEGLX11::Update()
{
  m_render_window->UpdateDimensions();
  m_backbuffer_width = m_render_window->GetWidth();
  m_backbuffer_height = m_render_window->GetHeight();
}

EGLDisplay GLContextEGLX11::OpenEGLDisplay()
{
  return eglGetDisplay(static_cast<Display*>(m_wsi.display_connection));
}

EGLNativeWindowType GLContextEGLX11::GetEGLNativeWindow(EGLConfig config)
{
  EGLint vid;
  eglGetConfigAttrib(m_egl_display, config, EGL_NATIVE_VISUAL_ID, &vid);

  XVisualInfo visTemplate = {};
  visTemplate.visualid = vid;

  int nVisuals;
  XVisualInfo* vi = XGetVisualInfo(static_cast<Display*>(m_wsi.display_connection), VisualIDMask,
                                   &visTemplate, &nVisuals);

  if (m_render_window)
    m_render_window.reset();

  m_render_window = GLX11Window::Create(static_cast<Display*>(m_wsi.display_connection),
                                        reinterpret_cast<Window>(m_wsi.render_surface), vi);
  m_backbuffer_width = m_render_window->GetWidth();
  m_backbuffer_height = m_render_window->GetHeight();

  XFree(vi);

  return reinterpret_cast<EGLNativeWindowType>(m_render_window->GetWindow());
}
