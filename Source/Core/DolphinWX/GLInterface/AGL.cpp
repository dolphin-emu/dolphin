// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/panel.h>

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "DolphinWX/GLInterface/GLInterface.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

void cInterfaceAGL::Swap()
{
	[GLWin.cocoaCtx flushBuffer];
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceAGL::Create(void *&window_handle)
{
	int _tx, _ty, _twidth, _theight;
	Host_GetRenderWindowSize(_tx, _ty, _twidth, _theight);

	GLWin.cocoaWin = (NSView*)(((wxPanel*)window_handle)->GetHandle());

	// Enable high-resolution display support.
	[GLWin.cocoaWin setWantsBestResolutionOpenGLSurface:YES];

	NSWindow *window = [GLWin.cocoaWin window];

	float scale = [window backingScaleFactor];
	_twidth *= scale;
	_theight *= scale;

	// Control window size and picture scaling
	s_backbuffer_width = _twidth;
	s_backbuffer_height = _theight;

	NSOpenGLPixelFormatAttribute attr[] = { NSOpenGLPFADoubleBuffer, NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core, NSOpenGLPFAAccelerated, 0 };
	NSOpenGLPixelFormat *fmt = [[NSOpenGLPixelFormat alloc]
		initWithAttributes: attr];
	if (fmt == nil) {
		ERROR_LOG(VIDEO, "failed to create pixel format");
		return false;
	}

	GLWin.cocoaCtx = [[NSOpenGLContext alloc]
		initWithFormat: fmt shareContext: nil];
	[fmt release];
	if (GLWin.cocoaCtx == nil) {
		ERROR_LOG(VIDEO, "failed to create context");
		return false;
	}

	if (GLWin.cocoaWin == nil) {
		ERROR_LOG(VIDEO, "failed to create window");
		return false;
	}

	[window makeFirstResponder:GLWin.cocoaWin];
	[GLWin.cocoaCtx setView: GLWin.cocoaWin];
	[window makeKeyAndOrderFront: nil];

	return true;
}

bool cInterfaceAGL::MakeCurrent()
{
	[GLWin.cocoaCtx makeCurrentContext];
	return true;
}

bool cInterfaceAGL::ClearCurrent()
{
	// not tested at all
	//clearCurrentContext();
	return true;
}

// Close backend
void cInterfaceAGL::Shutdown()
{
	[GLWin.cocoaCtx clearDrawable];
	[GLWin.cocoaCtx release];
	GLWin.cocoaCtx = nil;
}

void cInterfaceAGL::Update()
{
	NSWindow *window = [GLWin.cocoaWin window];
	NSSize size = [GLWin.cocoaWin frame].size;

	float scale = [window backingScaleFactor];
	size.width *= scale;
	size.height *= scale;

	if (s_backbuffer_width == size.width &&
	    s_backbuffer_height == size.height)
		return;

	s_backbuffer_width = size.width;
	s_backbuffer_height = size.height;

	[GLWin.cocoaCtx update];
}

void cInterfaceAGL::SwapInterval(int interval)
{
	[GLWin.cocoaCtx setValues:(GLint *)&interval forParameter:NSOpenGLCPSwapInterval];
}

