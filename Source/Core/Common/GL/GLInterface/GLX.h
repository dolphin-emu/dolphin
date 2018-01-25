// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <GL/glx.h>
#include <GL/glxext.h>
#include <memory>
#include <string>
#include <vector>

#include "Common/GL/GLInterface/X11_Util.h"
#include "Common/GL/GLInterfaceBase.h"

class cInterfaceGLX : public cInterfaceBase
{
private:
  Window m_host_window;
  cX11Window XWindow;
  Display *dpy, *dpy_offscreen;
  Window win;//, win_offscreen;
  GLXContext ctx, ctx_offscreen;
  GLXFBConfig fbconfig;
  bool m_has_handle;
  bool m_supports_pbuffer = false;
  GLXPbufferSGIX m_pbuffer = 0;
  std::vector<int> m_attribs;

  bool CreateWindowSurface();
  void DestroyWindowSurface();

public:
  const Display* getDisplay() {return dpy;};
  friend class cX11Window;
  void SwapInterval(int Interval) override;
  void Swap() override;
  void* GetFuncAddress(const std::string& name) override;
  bool Create(void *window_handle, bool stereo, bool core) override;
  bool CreateOffscreen();
  bool Create(cInterfaceBase* main_context) override;
  bool MakeCurrent() override;
  bool MakeCurrentOffscreen();
  bool ClearCurrent() override;
  bool ClearCurrentOffscreen();

  void Shutdown() override;
  void ShutdownOffscreen();
  std::unique_ptr<cInterfaceBase> CreateSharedContext() override;
};
