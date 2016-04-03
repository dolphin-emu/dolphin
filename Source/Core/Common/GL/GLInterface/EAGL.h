// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __APPLE__
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/gltypes.h>
#import <OpenGLES/EAGL.h>
#endif

#include "Common/GL/GLInterfaceBase.h"

class cInterfaceEAGL : public cInterfaceBase
{
private:
    CAEAGLLayer *layer;
    EAGLContext *context;
    GLuint framebuffer;
    GLuint renderbuffer;
public:
    void Swap();
    bool Create(void *window_handle, bool core);
    bool MakeCurrent();
    bool ClearCurrent();
    u32 DisplayFramebuffer();
    void Shutdown();
    void Update();

};
