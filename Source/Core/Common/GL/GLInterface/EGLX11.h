// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <X11/Xlib.h>

#include "Common/GL/GLInterface/EGL.h"
#include "Common/GL/GLX11Window.h"

class GLContextEGLX11 : public GLContextEGL
{
public:
  ~GLContextEGLX11() override;

protected:
  EGLDisplay OpenEGLDisplay() override;
  EGLNativeWindowType GetEGLNativeWindow(EGLConfig config) override;

  Display* m_display = nullptr;
  std::unique_ptr<GLX11Window> m_x_window;
};
