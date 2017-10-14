// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <sstream>

#include "Common/GL/GLInterface/GLX.h"
#include "Common/Logging/Log.h"

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSPROC)(Display*, GLXFBConfig, GLXContext, Bool,
                                                     const int*);
typedef int (*PFNGLXSWAPINTERVALSGIPROC)(int interval);

static PFNGLXCREATECONTEXTATTRIBSPROC glXCreateContextAttribs = nullptr;
static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = nullptr;

static PFNGLXCREATEGLXPBUFFERSGIXPROC glXCreateGLXPbufferSGIX = nullptr;
static PFNGLXDESTROYGLXPBUFFERSGIXPROC glXDestroyGLXPbufferSGIX = nullptr;

static bool s_glxError;
static int ctxErrorHandler(Display* dpy, XErrorEvent* ev)
{
  s_glxError = true;
  return 0;
}

void cInterfaceGLX::SwapInterval(int Interval)
{
  if (glXSwapIntervalSGI && m_has_handle)
    glXSwapIntervalSGI(Interval);
  else
    ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
}
void* cInterfaceGLX::GetFuncAddress(const std::string& name)
{
  return (void*)glXGetProcAddress((const GLubyte*)name.c_str());
}

void cInterfaceGLX::Swap()
{
  glXSwapBuffers(dpy, win);
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceGLX::Create(void* window_handle, bool stereo, bool core)
{
  m_has_handle = !!window_handle;
  m_host_window = (Window)window_handle;

  dpy = XOpenDisplay(nullptr);
  int screen = DefaultScreen(dpy);

  // checking glx version
  int glxMajorVersion, glxMinorVersion;
  glXQueryVersion(dpy, &glxMajorVersion, &glxMinorVersion);
  if (glxMajorVersion < 1 || (glxMajorVersion == 1 && glxMinorVersion < 4))
  {
    ERROR_LOG(VIDEO, "glX-Version %d.%d detected, but need at least 1.4", glxMajorVersion,
              glxMinorVersion);
    return false;
  }

  // loading core context creation function
  glXCreateContextAttribs =
      (PFNGLXCREATECONTEXTATTRIBSPROC)GetFuncAddress("glXCreateContextAttribsARB");
  if (!glXCreateContextAttribs)
  {
    ERROR_LOG(VIDEO,
              "glXCreateContextAttribsARB not found, do you support GLX_ARB_create_context?");
    return false;
  }

  // choosing framebuffer
  int visual_attribs[] = {GLX_X_RENDERABLE,
                          True,
                          GLX_DRAWABLE_TYPE,
                          GLX_WINDOW_BIT,
                          GLX_X_VISUAL_TYPE,
                          GLX_TRUE_COLOR,
                          GLX_RED_SIZE,
                          8,
                          GLX_GREEN_SIZE,
                          8,
                          GLX_BLUE_SIZE,
                          8,
                          GLX_DEPTH_SIZE,
                          0,
                          GLX_STENCIL_SIZE,
                          0,
                          GLX_DOUBLEBUFFER,
                          True,
                          GLX_STEREO,
                          stereo ? True : False,
                          None};
  int fbcount = 0;
  GLXFBConfig* fbc = glXChooseFBConfig(dpy, screen, visual_attribs, &fbcount);
  if (!fbc || !fbcount)
  {
    ERROR_LOG(VIDEO, "Failed to retrieve a framebuffer config");
    return false;
  }
  fbconfig = *fbc;
  XFree(fbc);

  s_glxError = false;
  XErrorHandler oldHandler = XSetErrorHandler(&ctxErrorHandler);

  // Create a GLX context.
  // We try to get a 4.0 core profile, else we try 3.3, else try it with anything we get.
  std::array<int, 9> context_attribs = {
      {GLX_CONTEXT_MAJOR_VERSION_ARB, 4, GLX_CONTEXT_MINOR_VERSION_ARB, 0,
       GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, GLX_CONTEXT_FLAGS_ARB,
       GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, None}};
  ctx = nullptr;
  if (core)
  {
    ctx = glXCreateContextAttribs(dpy, fbconfig, 0, True, &context_attribs[0]);
    XSync(dpy, False);
    m_attribs.insert(m_attribs.end(), context_attribs.begin(), context_attribs.end());
  }
  if (core && (!ctx || s_glxError))
  {
    std::array<int, 9> context_attribs_33 = {
        {GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB, 3,
         GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, GLX_CONTEXT_FLAGS_ARB,
         GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, None}};
    s_glxError = false;
    ctx = glXCreateContextAttribs(dpy, fbconfig, 0, True, &context_attribs_33[0]);
    XSync(dpy, False);
    m_attribs.clear();
    m_attribs.insert(m_attribs.end(), context_attribs_33.begin(), context_attribs_33.end());
  }
  if (!ctx || s_glxError)
  {
    std::array<int, 5> context_attribs_legacy = {
        {GLX_CONTEXT_MAJOR_VERSION_ARB, 1, GLX_CONTEXT_MINOR_VERSION_ARB, 0, None}};
    s_glxError = false;
    ctx = glXCreateContextAttribs(dpy, fbconfig, 0, True, &context_attribs_legacy[0]);
    XSync(dpy, False);
    m_attribs.clear();
    m_attribs.insert(m_attribs.end(), context_attribs_legacy.begin(), context_attribs_legacy.end());
  }
  if (!ctx || s_glxError)
  {
    ERROR_LOG(VIDEO, "Unable to create GL context.");
    XSetErrorHandler(oldHandler);
    return false;
  }

  std::string tmp;
  std::istringstream buffer(glXQueryExtensionsString(dpy, screen));
  while (buffer >> tmp)
  {
    if (tmp == "GLX_SGIX_pbuffer")
      m_supports_pbuffer = true;
  }

  if (m_supports_pbuffer)
  {
    // Get the function pointers we require
    glXCreateGLXPbufferSGIX =
        (PFNGLXCREATEGLXPBUFFERSGIXPROC)GetFuncAddress("glXCreateGLXPbufferSGIX");
    glXDestroyGLXPbufferSGIX =
        (PFNGLXDESTROYGLXPBUFFERSGIXPROC)GetFuncAddress("glXDestroyGLXPbufferSGIX");
  }

  if (!CreateWindowSurface())
  {
    ERROR_LOG(VIDEO, "Error: CreateWindowSurface failed\n");
    XSetErrorHandler(oldHandler);
    return false;
  }

  XSetErrorHandler(oldHandler);
  return true;
}

bool cInterfaceGLX::Create(cInterfaceBase* main_context)
{
  cInterfaceGLX* glx_context = static_cast<cInterfaceGLX*>(main_context);

  m_has_handle = false;
  m_supports_pbuffer = glx_context->m_supports_pbuffer;
  dpy = glx_context->dpy;
  fbconfig = glx_context->fbconfig;
  s_glxError = false;
  XErrorHandler oldHandler = XSetErrorHandler(&ctxErrorHandler);

  ctx = glXCreateContextAttribs(dpy, fbconfig, glx_context->ctx, True, &glx_context->m_attribs[0]);
  XSync(dpy, False);

  if (!ctx || s_glxError)
  {
    ERROR_LOG(VIDEO, "Unable to create GL context.");
    XSetErrorHandler(oldHandler);
    return false;
  }

  if (m_supports_pbuffer && !CreateWindowSurface())
  {
    ERROR_LOG(VIDEO, "Error: CreateWindowSurface failed\n");
    XSetErrorHandler(oldHandler);
    return false;
  }

  XSetErrorHandler(oldHandler);
  return true;
}

std::unique_ptr<cInterfaceBase> cInterfaceGLX::CreateSharedContext()
{
  std::unique_ptr<cInterfaceBase> context = std::make_unique<cInterfaceGLX>();
  if (!context->Create(this))
    return nullptr;
  return context;
}

bool cInterfaceGLX::CreateWindowSurface()
{
  if (m_has_handle)
  {
    // Get an appropriate visual
    XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbconfig);

    XWindow.Initialize(dpy);

    XWindowAttributes attribs;
    if (!XGetWindowAttributes(dpy, m_host_window, &attribs))
    {
      ERROR_LOG(VIDEO, "Window attribute retrieval failed");
      return false;
    }

    s_backbuffer_width = attribs.width;
    s_backbuffer_height = attribs.height;

    win = XWindow.CreateXWindow(m_host_window, vi);
    XFree(vi);
  }
  else if (m_supports_pbuffer)
  {
    win = m_pbuffer = glXCreateGLXPbufferSGIX(dpy, fbconfig, 1, 1, nullptr);
    if (!m_pbuffer)
      return false;
  }

  return true;
}

void cInterfaceGLX::DestroyWindowSurface()
{
  if (m_has_handle)
  {
    XWindow.DestroyXWindow();
  }
  else if (m_supports_pbuffer && m_pbuffer)
  {
    glXDestroyGLXPbufferSGIX(dpy, m_pbuffer);
    m_pbuffer = 0;
  }
}

bool cInterfaceGLX::MakeCurrent()
{
  bool success = glXMakeCurrent(dpy, win, ctx);
  if (success && !glXSwapIntervalSGI)
  {
    // load this function based on the current bound context
    glXSwapIntervalSGI =
        (PFNGLXSWAPINTERVALSGIPROC)GLInterface->GetFuncAddress("glXSwapIntervalSGI");
  }
  return success;
}

bool cInterfaceGLX::ClearCurrent()
{
  return glXMakeCurrent(dpy, None, nullptr);
}

// Close backend
void cInterfaceGLX::Shutdown()
{
  DestroyWindowSurface();
  if (ctx)
  {
    glXDestroyContext(dpy, ctx);

    // Don't close the display connection if we are a shared context.
    // Saves doing reference counting on this object, and the main context will always
    // be shut down last anyway.
    if (m_has_handle)
    {
      XCloseDisplay(dpy);
      ctx = nullptr;
    }
  }
}
