// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/AGL.h"
#include "Common/Logging/Log.h"

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
    ERROR_LOG(VIDEO, "failed to get NSWindow");
    return false;
  }

  (void)UpdateCachedDimensions(view, width, height);

  [window makeFirstResponder:view];
  [context setView:view];
  [window makeKeyAndOrderFront:nil];

  return true;
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
bool GLContextAGL::Initialize(void* window_handle, bool stereo, bool core)
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
  m_opengl_mode = Mode::OpenGL;
  return AttachContextToView(m_context, m_view, &m_backbuffer_width, &m_backbuffer_height);
}

bool GLContextAGL::Initialize(GLContext* main_context)
{
  GLContextAGL* agl_context = static_cast<GLContextAGL*>(main_context);
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

std::unique_ptr<GLContext> GLContextAGL::CreateSharedContext()
{
  std::unique_ptr<GLContextAGL> context = std::make_unique<GLContextAGL>();
  if (!context->Initialize(this))
    return nullptr;
  return context;
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

// Close backend
void GLContextAGL::Shutdown()
{
  [m_context clearDrawable];
  [m_context release];
  m_context = nil;
  [m_pixel_format release];
  m_pixel_format = nil;
}

void GLContextAGL::Update()
{
  if (!m_view)
    return;

  if (UpdateCachedDimensions(m_view, &m_backbuffer_width, &m_backbuffer_height))
    [m_context update];
}

void GLContextAGL::SwapInterval(int interval)
{
  [m_context setValues:static_cast<GLint*>(&interval) forParameter:NSOpenGLCPSwapInterval];
}
