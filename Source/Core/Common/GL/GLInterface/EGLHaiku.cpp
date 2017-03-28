// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EGLHaiku.h"

#include <Window.h>

EGLDisplay cInterfaceEGLHaiku::OpenDisplay()
{
  return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLNativeWindowType cInterfaceEGLHaiku::InitializePlatform(EGLNativeWindowType host_window,
                                                           EGLConfig config)
{
  EGLint format;
  eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &format);

  BWindow* window = reinterpret_cast<BWindow*>(host_window);
  const int width = window->Size().width;
  const int height = window->Size().height;
  GLInterface->SetBackBufferDimensions(width, height);

  return host_window;
}

void cInterfaceEGLHaiku::ShutdownPlatform()
{
}
