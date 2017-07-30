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
  [m_context flushBuffer];
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceAGL::Create(void* window_handle, bool stereo, bool core)
{
  NSOpenGLPixelFormatAttribute attr[] = {
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAOpenGLProfile,
      core ? NSOpenGLProfileVersion3_2Core : NSOpenGLProfileVersionLegacy,
      NSOpenGLPFAAccelerated,
      stereo ? NSOpenGLPFAStereo : static_cast<NSOpenGLPixelFormatAttribute>(0),
      0};
  m_pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
  if (m_pixel_format == nil)
  {
    ERROR_LOG(VIDEO, "failed to create pixel format");
    return false;
  }

  m_context = [[NSOpenGLContext alloc] initWithFormat:m_pixel_format shareContext:nil];
  if (m_context == nil)
  {
    ERROR_LOG(VIDEO, "failed to create context");
    return false;
  }

  if (!window_handle)
    return true;

  m_view = static_cast<NSView*>(window_handle);
  return AttachContextToView(m_context, m_view, &s_backbuffer_width, &s_backbuffer_height);
}

bool cInterfaceAGL::Create(cInterfaceBase* main_context)
{
  cInterfaceAGL* agl_context = static_cast<cInterfaceAGL*>(main_context);
  NSOpenGLPixelFormat* pixel_format = agl_context->m_pixel_format;
  NSOpenGLContext* share_context = agl_context->m_context;

  m_context = [[NSOpenGLContext alloc] initWithFormat:pixel_format shareContext:share_context];
  if (m_context == nil)
  {
    ERROR_LOG(VIDEO, "failed to create shared context");
    return false;
  }

  return true;
}

std::unique_ptr<cInterfaceBase> cInterfaceAGL::CreateSharedContext()
{
  std::unique_ptr<cInterfaceBase> context = std::make_unique<cInterfaceAGL>();
  if (!context->Create(this))
    return nullptr;
  return context;
}

bool cInterfaceAGL::MakeCurrent()
{
  [m_context makeCurrentContext];
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
  [m_context clearDrawable];
  [m_context release];
  m_context = nil;
  [m_pixel_format release];
  m_pixel_format = nil;
}

void cInterfaceAGL::Update()
{
  if (!m_view)
    return;

  if (UpdateCachedDimensions(m_view, &s_backbuffer_width, &s_backbuffer_height))
    [m_context update];
}

void cInterfaceAGL::SwapInterval(int interval)
{
  [m_context setValues:static_cast<GLint*>(&interval) forParameter:NSOpenGLCPSwapInterval];
}
