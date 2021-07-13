// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// This define will silence all "OpenGL is deprecated, use Metal" warnings.
#define GL_SILENCE_DEPRECATION 1

#if defined(__APPLE__) && defined(__OBJC__)
#import <AppKit/AppKit.h>
#else
struct NSOpenGLContext;
struct NSOpenGLPixelFormat;
struct NSView;
#endif

#include "Common/GL/GLContext.h"

class GLContextAGL final : public GLContext
{
public:
  ~GLContextAGL() override;

  bool IsHeadless() const override;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void Update() override;

  void Swap() override;
  void SwapInterval(int interval) override;

protected:
  bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override;

  NSView* m_view = nullptr;
  NSOpenGLContext* m_context = nullptr;
  NSOpenGLPixelFormat* m_pixel_format = nullptr;
};
