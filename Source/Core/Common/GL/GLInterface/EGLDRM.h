// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string>
#include <vector>

#include "Common/GL/GLContext.h"

struct EGLContextData
{
  EGLContext ctx;
  EGLSurface surf;
  EGLDisplay dpy;
  EGLConfig config;
  int interval;

  unsigned major;
  unsigned minor;
};

class GLContextEGLDRM : public GLContext
{
public:
  ~GLContextEGLDRM() override;

  bool IsHeadless() const override;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void UpdateSurface(void* window_handle) override;

  void Swap() override;
  void SwapInterval(int interval) override;

  void* GetFuncAddress(const std::string& name) override;

protected:
  bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override;

  bool CreateWindowSurface();
  void DestroyWindowSurface();
  void DestroyContext();

  bool m_supports_surfaceless = false;
  EGLContextData* m_egl;
};
