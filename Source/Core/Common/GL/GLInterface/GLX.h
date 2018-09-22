// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <GL/glx.h>
#include <GL/glxext.h>
#include <memory>
#include <string>
#include <vector>

#include "Common/GL/GLContext.h"
#include "Common/GL/GLX11Window.h"

class GLContextGLX final : public GLContext
{
public:
  ~GLContextGLX() override;

  bool IsHeadless() const override;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void Update() override;

  void SwapInterval(int Interval) override;
  void Swap() override;

  void* GetFuncAddress(const std::string& name) override;

protected:
  bool Initialize(void* display_handle, void* window_handle, bool stereo, bool core) override;

  Display* m_display = nullptr;
  std::unique_ptr<GLX11Window> m_render_window;

  GLXDrawable m_drawable = {};
  GLXContext m_context = nullptr;
  GLXFBConfig m_fbconfig = {};
  bool m_supports_pbuffer = false;
  GLXPbufferSGIX m_pbuffer = 0;
  std::vector<int> m_attribs;

  bool CreateWindowSurface(Window window_handle);
  void DestroyWindowSurface();
};
