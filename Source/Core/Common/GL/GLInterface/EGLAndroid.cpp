// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/EGLAndroid.h"

#include <android/native_window.h>

EGLDisplay GLContextEGLAndroid::OpenEGLDisplay()
{
  return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLNativeWindowType GLContextEGLAndroid::GetEGLNativeWindow(EGLConfig config)
{
  EGLint format;
  eglGetConfigAttrib(m_egl_display, config, EGL_NATIVE_VISUAL_ID, &format);
  ANativeWindow_setBuffersGeometry(static_cast<ANativeWindow*>(m_wsi.render_surface), 0, 0, format);
  m_backbuffer_width = ANativeWindow_getWidth(static_cast<ANativeWindow*>(m_wsi.render_surface));
  m_backbuffer_height = ANativeWindow_getHeight(static_cast<ANativeWindow*>(m_wsi.render_surface));
  return static_cast<EGLNativeWindowType>(m_wsi.render_surface);
}
