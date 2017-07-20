// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__APPLE__) && defined(__OBJC__)
#import <AppKit/AppKit.h>
#else
struct NSOpenGLContext;
struct NSOpenGLPixelFormat;
struct NSView;
#endif

#include "Common/GL/GLInterfaceBase.h"

class cInterfaceAGL : public cInterfaceBase
{
public:
  void Swap() override;
  bool Create(void* window_handle, bool stereo, bool core) override;
  bool Create(cInterfaceBase* main_context) override;
  bool MakeCurrent() override;
  bool ClearCurrent() override;
  void Shutdown() override;
  void Update() override;
  void SwapInterval(int interval) override;
  std::unique_ptr<cInterfaceBase> CreateSharedContext() override;

private:
  NSView* m_view = nullptr;
  NSOpenGLContext* m_context = nullptr;
  NSOpenGLPixelFormat* m_pixel_format = nullptr;
};
