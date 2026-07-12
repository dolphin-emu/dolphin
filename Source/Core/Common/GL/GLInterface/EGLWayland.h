// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/DynamicLibrary.h"
#include "Common/GL/GLInterface/EGL.h"

struct wl_egl_window;
struct wl_surface;

// EGL context for Wayland (aligned with dolphin-emu/dolphin#14652).
class GLContextEGLWayland final : public GLContextEGL
{
public:
  ~GLContextEGLWayland() override;

protected:
  EGLDisplay OpenEGLDisplay() override;
  EGLNativeWindowType GetEGLNativeWindow(EGLConfig config) override;

private:
  Common::DynamicLibrary m_wayland_lib;
  wl_egl_window* (*m_wl_egl_window_create)(wl_surface* surface, int width, int height) = nullptr;
  void (*m_wl_egl_window_destroy)(wl_egl_window* window) = nullptr;
  wl_egl_window* m_window = nullptr;
};
