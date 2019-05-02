// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/GL/GLInterface/EGL.h"

class GLContextEGLWayland : public GLContextEGL
{
public:
  ~GLContextEGLWayland();

  void UpdateDimensions(int window_width, int window_height) override;

protected:
  EGLDisplay OpenEGLDisplay() override;
  EGLNativeWindowType GetEGLNativeWindow(EGLConfig config) override;
};
