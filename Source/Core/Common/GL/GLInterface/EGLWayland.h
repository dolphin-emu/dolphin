// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <wayland-client-protocol.h>
#include "Common/DynamicLibrary.h"
#include "Common/GL/GLInterface/EGL.h"

struct wl_egl_window;
struct wl_surface;

class GLContextEGLWayland final : public GLContextEGL
{
public:
  ~GLContextEGLWayland() override;

  void Update(u32 width, u32 height) override;

protected:
  EGLDisplay OpenEGLDisplay() override;
  EGLNativeWindowType GetEGLNativeWindow(EGLConfig config) override;
  Common::DynamicLibrary m_wayland_lib;
  wl_egl_window* (*m_wl_egl_window_create)(wl_surface* surface, int width, int height);
  void (*m_wl_egl_window_destroy)(wl_egl_window* window);
  void (*m_wl_egl_window_resize)(wl_egl_window* window, int width, int height, int dx, int dy);
  wl_egl_window* m_window = nullptr;
};
