// Copyright 2012 Dolphin Emulator Project
// Copyright 2016 Will Cobb
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdlib>
#include <sstream>
#include <vector>

#import <GLKit/GLKit.h>

#include "Common/GL/GLInterface/IGL.h"
#include "Common/Logging/Log.h"
#include "Common/GL/GLUtil.h"



EAGLContext *context;
GLKView *glkView;
int framenum = 0;

void cInterfaceIGL::Swap()
{
    //NSLog(@"Frame: %d", framenum++);
    [context presentRenderbuffer:GL_RENDERBUFFER];
}
void cInterfaceIGL::SwapInterval(int Interval)
{
    
}

GLInterfaceMode cInterfaceIGL::GetMode()
{
    return GLInterfaceMode::MODE_OPENGLES3;
}

void cInterfaceIGL::DetectMode()
{
    s_opengl_mode = GLInterfaceMode::MODE_OPENGLES3;
}

bool cInterfaceIGL::Create(void *window_handle, bool core)
{
    context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    [EAGLContext setCurrentContext: context];
    glkView = (GLKView *)window_handle;
    glkView.context = context;
    
    //CAEAGLLayer * eaglLayer = (CAEAGLLayer*) glkView.layer;
    //eaglLayer.drawableProperties = @{kEAGLDrawablePropertyRetainedBacking : @(YES)};
    
    s_backbuffer_width = glkView.frame.size.width * 2;
    s_backbuffer_height = glkView.frame.size.height * 2;
    
    return true;
}

bool bufferStorageCreated = false;

void cInterfaceIGL::Update()
{
    if (bufferStorageCreated) {
        
        
    }
}

void cInterfaceIGL::Prepare()
{
    [glkView bindDrawable];
    if (!bufferStorageCreated) {
        if (![context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)glkView.layer]) {
            NSLog(@"Unable to create renderbufferstorage");
        }
        bufferStorageCreated = true;
    }
}


bool cInterfaceIGL::Create(cInterfaceBase* main_context)
{
    NSLog(@"Create(cInterfaceBase* main_context)");

	return true;
}

bool cInterfaceIGL::CreateWindowSurface()
{
    NSLog(@"CreateWindowSurface()");
    
	return true;
}

void cInterfaceIGL::DestroyWindowSurface()
{
//	if (egl_surf != EGL_NO_SURFACE && !eglDestroySurface(egl_dpy, egl_surf))
//		NOTICE_LOG(VIDEO, "Could not destroy window surface.");
//	egl_surf = EGL_NO_SURFACE;
}

bool cInterfaceIGL::MakeCurrent()
{
    [EAGLContext setCurrentContext: context];
    return true;
}

void cInterfaceIGL::UpdateHandle(void* window_handle)
{
//	m_host_window = (EGLNativeWindowType)window_handle;
//	m_has_handle = !!window_handle;
}

void cInterfaceIGL::UpdateSurface()
{
//	ClearCurrent();
//	DestroyWindowSurface();
//	CreateWindowSurface();
//	MakeCurrent();
}

bool cInterfaceIGL::ClearCurrent()
{
    [EAGLContext setCurrentContext:nil];
    return true;
}

// Close backend
void cInterfaceIGL::Shutdown()
{
//	ShutdownPlatform();
}
