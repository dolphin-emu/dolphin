// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EAGL.h"
#include "Common/GL/GLExtensions/GLExtensions.h"
#include "Common/Logging/Log.h"

void cInterfaceEAGL::Swap()
{
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];
}

bool cInterfaceEAGL::Create(void *window_handle, bool core)
{
    s_opengl_mode = GLInterfaceMode::MODE_OPENGLES3;

    layer = reinterpret_cast<CAEAGLLayer*>(window_handle);
    [layer retain];

    CGSize size = layer.frame.size;

    float scale = layer.contentsScale;
    size.width *= scale;
    size.height *= scale;

    // Control window size and picture scaling
    s_backbuffer_width = size.width;
    s_backbuffer_height = size.height;

    context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (context == nil)
    {
        ERROR_LOG(VIDEO, "failed to create context");
        return false;
    }

    if (layer == nil)
    {
        ERROR_LOG(VIDEO, "failed to get layer");
        return false;
    }

    if (!MakeCurrent())
    {
        ERROR_LOG(VIDEO, "failed to make current");
        return false;
    }

    if (!GLExtensions::Init())
    {
        ERROR_LOG(VIDEO, "failed to init");
        return false;
    }

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ClearCurrent();

    return true;
}

bool cInterfaceEAGL::MakeCurrent()
{
    return [EAGLContext setCurrentContext:context];
}

bool cInterfaceEAGL::ClearCurrent()
{
    return [EAGLContext setCurrentContext:nil];
}

u32 cInterfaceEAGL::DisplayFramebuffer()
{
    return framebuffer;
}

void cInterfaceEAGL::Shutdown()
{
    [layer release];
    [context release];
    context = nil;
}

void cInterfaceEAGL::Update()
{
    CGSize size = layer.frame.size;

    float scale = layer.contentsScale;
    size.width *= scale;
    size.height *= scale;

    if (s_backbuffer_width == size.width &&
        s_backbuffer_height == size.height)
        return;

    s_backbuffer_width = size.width;
    s_backbuffer_height = size.height;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glDeleteRenderbuffers(1, &renderbuffer);
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

