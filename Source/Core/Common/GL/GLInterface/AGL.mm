// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLInterface/AGL.h"
#include "Common/Logging/Log.h"

// UpdateCachedDimensions and AttachContextToView contain calls to UI APIs, so they must only be
// called from the main thread or they risk crashing!

static bool UpdateCachedDimensions(NSView* view, u32* width, u32* height)
{
  NSWindow* window = [view window];
  NSSize size = [view frame].size;

  const CGFloat scale = [window backingScaleFactor];
  u32 new_width = static_cast<u32>(size.width * scale);
  u32 new_height = static_cast<u32>(size.height * scale);

  if (*width == new_width && *height == new_height)
    return false;

  *width = new_width;
  *height = new_height;
  return true;
}

static bool AttachContextToView(NSOpenGLContext* context, NSView* view, u32* width, u32* height)
{
  // Enable high-resolution display support.
  [view setWantsBestResolutionOpenGLSurface:YES];

  NSWindow* window = [view window];
  if (window == nil)
  {
    ERROR_LOG_FMT(VIDEO, "failed to get NSWindow");
    return false;
  }

  (void)UpdateCachedDimensions(view, width, height);

  [context setView:view];

  return true;
}

GLContextAGL::~GLContextAGL()
{
  if ([NSOpenGLContext currentContext] == m_context)
    [NSOpenGLContext clearCurrentContext];

  if (m_context)
  {
    [m_context clearDrawable];
    [m_context release];
  }
  if (m_pixel_format)
    [m_pixel_format release];
}

bool GLContextAGL::IsHeadless() const
{
  return !m_view;
}

void GLContextAGL::Swap()
{
  [m_context flushBuffer];
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool GLContextAGL::Initialize(const WindowSystemInfo& wsi, bool stereo, bool core)
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
    ERROR_LOG_FMT(VIDEO, "failed to create pixel format");
    return false;
  }

  m_context = [[NSOpenGLContext alloc] initWithFormat:m_pixel_format shareContext:nil];
  if (m_context == nil)
  {
    ERROR_LOG_FMT(VIDEO, "failed to create context");
    return false;
  }

  if (!wsi.render_surface)
    return true;

  m_view = static_cast<NSView*>(wsi.render_surface);
  m_opengl_mode = Mode::OpenGL;

  __block bool success;
  if ([NSThread isMainThread])
  {
    success = AttachContextToView(m_context, m_view, &m_backbuffer_width, &m_backbuffer_height);
  }
  else
  {
    dispatch_sync(dispatch_get_main_queue(), ^{
      success = AttachContextToView(m_context, m_view, &m_backbuffer_width, &m_backbuffer_height);
    });
  }

  if (!success)
  {
    return false;
  }

  [m_context makeCurrentContext];
  return true;
}

std::unique_ptr<GLContext> GLContextAGL::CreateSharedContext()
{
  NSOpenGLContext* new_agl_context = [[NSOpenGLContext alloc] initWithFormat:m_pixel_format
                                                                shareContext:m_context];
  if (new_agl_context == nil)
  {
    ERROR_LOG_FMT(VIDEO, "failed to create shared context");
    return nullptr;
  }

  std::unique_ptr<GLContextAGL> new_context = std::make_unique<GLContextAGL>();
  new_context->m_context = new_agl_context;
  new_context->m_pixel_format = m_pixel_format;
  [new_context->m_pixel_format retain];
  new_context->m_is_shared = true;
  return new_context;
}

bool GLContextAGL::MakeCurrent()
{
  [m_context makeCurrentContext];
  return true;
}

bool GLContextAGL::ClearCurrent()
{
  [NSOpenGLContext clearCurrentContext];
  return true;
}

void GLContextAGL::Update()
{
  if (!m_view)
    return;

  dispatch_sync(dispatch_get_main_queue(), ^{
    if (UpdateCachedDimensions(m_view, &m_backbuffer_width, &m_backbuffer_height))
    {
      [m_context update];
    }
  });
}

void GLContextAGL::SwapInterval(int interval)
{
  [m_context setValues:static_cast<GLint*>(&interval) forParameter:NSOpenGLCPSwapInterval];
}
