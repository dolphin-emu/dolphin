// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#import <OpenGLES/gltypes.h>

#if __OBJC__
#import <OpenGLES/EAGL.h>
#import <QuartzCore/QuartzCore.h>
#else
class CAEAGLLayer;
class EAGLContext;
#endif

#include "Common/GL/GLInterfaceBase.h"

class cInterfaceEAGL : public cInterfaceBase
{
private:
  CAEAGLLayer* layer;
  EAGLContext* context;
  GLuint framebuffer;
  GLuint renderbuffer;

public:
  void Swap() override;
  bool Create(void* window_handle, bool core) override;
  bool MakeCurrent() override;
  bool ClearCurrent() override;
  u32 DisplayFramebuffer() override;
  void Shutdown() override;
  void Update() override;
};
