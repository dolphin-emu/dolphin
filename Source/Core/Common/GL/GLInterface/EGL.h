// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string>
#include <vector>

#include "Common/GL/GLContext.h"

class GLContextEGL : public GLContext
{
public:
  virtual ~GLContextEGL() override;

  bool IsHeadless() const override;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void UpdateSurface(void* window_handle) override;

  void Swap() override;
  void SwapInterval(int interval) override;

  void* GetFuncAddress(const std::string& name) override;

protected:
  virtual EGLDisplay OpenEGLDisplay();
  virtual EGLNativeWindowType GetEGLNativeWindow(EGLConfig config);

  bool Initialize(void* display_handle, void* window_handle, bool stereo, bool core) override;

  bool CreateWindowSurface();
  void DestroyWindowSurface();
  void DetectMode(bool has_handle);
  void DestroyContext();

  void* m_host_display = nullptr;
  void* m_host_window = nullptr;

  EGLConfig m_config;
  bool m_supports_surfaceless = false;
  std::vector<int> m_attribs;

  EGLSurface m_egl_surface = EGL_NO_SURFACE;
  EGLContext m_egl_context = EGL_NO_CONTEXT;
  EGLDisplay m_egl_display = EGL_NO_DISPLAY;
};
