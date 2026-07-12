// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/GL/GLInterface/EGL.h"

// EGL context for Wayland (including webOS via SDL2 Wayland video).
class GLContextEGLWayland final : public GLContextEGL
{
protected:
  EGLDisplay OpenEGLDisplay() override;
  EGLNativeWindowType GetEGLNativeWindow(EGLConfig config) override;
};
