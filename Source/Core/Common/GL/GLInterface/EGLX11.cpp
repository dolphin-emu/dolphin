// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EGLX11.h"
#include "Common/Logging/Log.h"

GLContextEGLX11::~GLContextEGLX11()
{
  if (m_display)
    XCloseDisplay(m_display);
}

EGLDisplay GLContextEGLX11::OpenEGLDisplay()
{
  if (!m_display)
    m_display = XOpenDisplay(nullptr);

  return eglGetDisplay(m_display);
}

EGLNativeWindowType GLContextEGLX11::GetEGLNativeWindow(EGLConfig config)
{
  EGLint vid;
  eglGetConfigAttrib(m_egl_display, config, EGL_NATIVE_VISUAL_ID, &vid);

  XVisualInfo visTemplate = {};
  visTemplate.visualid = vid;

  int nVisuals;
  XVisualInfo* vi = XGetVisualInfo(m_display, VisualIDMask, &visTemplate, &nVisuals);

  if (m_x_window)
    m_x_window.reset();

  m_x_window = GLX11Window::Create(m_display, reinterpret_cast<Window>(m_host_window), vi);
  m_backbuffer_width = m_x_window->GetWidth();
  m_backbuffer_height = m_x_window->GetHeight();

  XFree(vi);

  return reinterpret_cast<EGLNativeWindowType>(m_x_window->GetWindow());
}
