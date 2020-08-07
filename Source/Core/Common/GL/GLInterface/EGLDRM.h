// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string>
#include <vector>

#include "Common/GL/GLContext.h"

typedef struct
{
  EGLContext ctx;
  EGLSurface surf;
  EGLDisplay dpy;
  EGLConfig config;
  int interval;

  unsigned major;
  unsigned minor;

} egl_ctx_data_t;

class GLContextEGLDRM : public GLContext
{
public:
  virtual ~GLContextEGLDRM() override;

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
  egl_ctx_data_t* m_egl;
};
