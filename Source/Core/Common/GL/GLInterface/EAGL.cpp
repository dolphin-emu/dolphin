// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EAGL.h"
#include "Common/GL/GLExtensions/GLExtensions.h"
#include "Common/Logging/Log.h"

void cInterfaceEAGL::Swap()
{
    NSLog(@"present renderbuffer");
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceEAGL::Create(void *window_handle, bool core)
{
	s_opengl_mode = GLInterfaceMode::MODE_OPENGLES3;

    NSLog(@"Creating EAGL handle");
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
        NSLog(@"failed to make current");
		ERROR_LOG(VIDEO, "failed to make current");
        return false;
    }

	if (!GLExtensions::Init())
	{
        NSLog(@"failed to init");
        return false;
    }

    NSLog(@"make currented %@", context);
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    NSLog(@"genned one framebuffer");

	glGenRenderbuffers(1, &renderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
	[context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ClearCurrent();
    NSLog(@"cleared");

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
    NSLog(@"asked for framebuffer %d", framebuffer);
    return framebuffer;
}

// Close backend
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

    NSLog(@"update to size %f %f", size.width, size.height);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glDeleteRenderbuffers(1, &renderbuffer);
	glGenRenderbuffers(1, &renderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
	[context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

