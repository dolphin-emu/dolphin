// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/OGL/GLInterface/AGL.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

void cInterfaceAGL::Swap()
{
	[cocoaCtx flushBuffer];
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceAGL::Create(void *window_handle)
{
	cocoaWin = reinterpret_cast<NSView*>(window_handle);
	NSSize size = [cocoaWin frame].size;

	// Enable high-resolution display support.
	[cocoaWin setWantsBestResolutionOpenGLSurface:YES];

	NSWindow *window = [cocoaWin window];

	float scale = [window backingScaleFactor];
	size.width *= scale;
	size.height *= scale;

	// Control window size and picture scaling
	s_backbuffer_width = size.width;
	s_backbuffer_height = size.height;

	NSOpenGLPixelFormatAttribute attr[] = { NSOpenGLPFADoubleBuffer, NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core, NSOpenGLPFAAccelerated, 0 };
	NSOpenGLPixelFormat *fmt = [[NSOpenGLPixelFormat alloc]
		initWithAttributes: attr];
	if (fmt == nil)
	{
		ERROR_LOG(VIDEO, "failed to create pixel format");
		return false;
	}

	cocoaCtx = [[NSOpenGLContext alloc] initWithFormat: fmt shareContext: nil];
	[fmt release];
	if (cocoaCtx == nil)
	{
		ERROR_LOG(VIDEO, "failed to create context");
		return false;
	}

	if (cocoaWin == nil)
	{
		ERROR_LOG(VIDEO, "failed to create window");
		return false;
	}

	[window makeFirstResponder:cocoaWin];
	[cocoaCtx setView: cocoaWin];
	[window makeKeyAndOrderFront: nil];

	return true;
}

bool cInterfaceAGL::MakeCurrent()
{
	[cocoaCtx makeCurrentContext];
	return true;
}

bool cInterfaceAGL::ClearCurrent()
{
	[NSOpenGLContext clearCurrentContext];
	return true;
}

// Close backend
void cInterfaceAGL::Shutdown()
{
	[cocoaCtx clearDrawable];
	[cocoaCtx release];
	cocoaCtx = nil;
}

void cInterfaceAGL::Update()
{
	NSWindow *window = [cocoaWin window];
	NSSize size = [cocoaWin frame].size;

	float scale = [window backingScaleFactor];
	size.width *= scale;
	size.height *= scale;

	if (s_backbuffer_width == size.width &&
	    s_backbuffer_height == size.height)
		return;

	s_backbuffer_width = size.width;
	s_backbuffer_height = size.height;

	[cocoaCtx update];
}

void cInterfaceAGL::SwapInterval(int interval)
{
	[cocoaCtx setValues:(GLint *)&interval forParameter:NSOpenGLCPSwapInterval];
}

