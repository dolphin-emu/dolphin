// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#import <OpenGLES/gltypes.h>

#if defined(__OBJC__)
#import <OpenGLES/EAGL.h>
#import <UIKit/UIKit.h>
#else
struct CAEAGLLayer;
struct EAGLContext;
#endif

#include "Common/GL/GLContext.h"

class GLContextEAGL final : public GLContext
{
public:
  ~GLContextEAGL() override;

  bool IsHeadless() const override;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void Update() override;

  void Swap() override;
  void SwapInterval(int interval) override;

  void* GetFuncAddress(const std::string& name) override;

  GLuint GetDefaultFramebuffer() override;

protected:
  bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override;

  CAEAGLLayer* m_layer = nullptr;
  EAGLContext* m_context = nullptr;
  bool m_framebuffer_created = false;

  GLuint m_framebuffer;
  GLuint m_color_renderbuffer;
};
