// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  ANativeWindow_setBuffersGeometry(static_cast<ANativeWindow*>(m_host_window), 0, 0, format);
  m_backbuffer_width = ANativeWindow_getWidth(static_cast<ANativeWindow*>(m_host_window));
  m_backbuffer_height = ANativeWindow_getHeight(static_cast<ANativeWindow*>(m_host_window));
  return static_cast<EGLNativeWindowType>(m_host_window);
}
