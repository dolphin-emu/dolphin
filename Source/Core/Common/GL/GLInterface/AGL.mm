// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/AGL.h"
#include "Common/Logging/Log.h"

static bool UpdateCachedDimensions(NSView* view, u32* width, u32* height)
{
  NSWindow* window = [view window];
  NSSize size = [view frame].size;

  float scale = [window backingScaleFactor];
  size.width *= scale;
  size.height *= scale;

  if (*width == size.width && *height == size.height)
    return false;

  *width = size.width;
  *height = size.height;

  return true;
}

static bool AttachContextToView(NSOpenGLContext* context, NSView* view, u32* width, u32* height)
{
  // Enable high-resolution display support.
  [view setWantsBestResolutionOpenGLSurface:YES];

  NSWindow* window = [view window];
  if (window == nil)
  {
    ERROR_LOG(VIDEO, "failed to get NSWindow");
    return false;
  }

  (void)UpdateCachedDimensions(view, width, height);

  [window makeFirstResponder:view];
  [context setView:view];
  [window makeKeyAndOrderFront:nil];

  return true;
}

void cInterfaceAGL::Swap()
{
  [cocoaCtx flushBuffer];
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceAGL::Create(void* window_handle, bool core)
{
  NSOpenGLPixelFormatAttribute attr[] = {NSOpenGLPFADoubleBuffer, NSOpenGLPFAOpenGLProfile,
                                         core ? NSOpenGLProfileVersion3_2Core :
                                                NSOpenGLProfileVersionLegacy,
                                         NSOpenGLPFAAccelerated, 0};
  NSOpenGLPixelFormat* fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
  if (fmt == nil)
  {
    ERROR_LOG(VIDEO, "failed to create pixel format");
    return false;
  }

  cocoaCtx = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil];
  [fmt release];
  if (cocoaCtx == nil)
  {
    ERROR_LOG(VIDEO, "failed to create context");
    return false;
  }

  if (!window_handle)
    return true;

  cocoaWin = static_cast<NSView*>(window_handle);
  return AttachContextToView(cocoaCtx, cocoaWin, &s_backbuffer_width, &s_backbuffer_height);
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
  if (!cocoaWin)
    return;

  if (UpdateCachedDimensions(cocoaWin, &s_backbuffer_width, &s_backbuffer_height))
    [cocoaCtx update];
}

void cInterfaceAGL::SwapInterval(int interval)
{
  [cocoaCtx setValues:(GLint*)&interval forParameter:NSOpenGLCPSwapInterval];
}
