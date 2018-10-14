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
typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display*, GLXDrawable, int);
typedef int (*PFNGLXSWAPINTERVALMESAPROC)(unsigned int);

static PFNGLXCREATECONTEXTATTRIBSPROC glXCreateContextAttribs = nullptr;
static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXTPtr = nullptr;
static PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESAPtr = nullptr;

static PFNGLXCREATEGLXPBUFFERSGIXPROC glXCreateGLXPbufferSGIX = nullptr;
static PFNGLXDESTROYGLXPBUFFERSGIXPROC glXDestroyGLXPbufferSGIX = nullptr;

static bool s_glxError;
static int ctxErrorHandler(Display* dpy, XErrorEvent* ev)
{
  s_glxError = true;
  return 0;
}

GLContextGLX::~GLContextGLX()
{
  DestroyWindowSurface();
  if (m_context)
  {
    if (glXGetCurrentContext() == m_context)
      glXMakeCurrent(m_display, None, nullptr);

    glXDestroyContext(m_display, m_context);
  }
}

bool GLContextGLX::IsHeadless() const
{
  return !m_render_window;
}

void GLContextGLX::SwapInterval(int Interval)
{
  if (!m_drawable)
    return;

  // Try EXT_swap_control, then MESA_swap_control.
  if (glXSwapIntervalEXTPtr)
    glXSwapIntervalEXTPtr(m_display, m_drawable, Interval);
  else if (glXSwapIntervalMESAPtr)
    glXSwapIntervalMESAPtr(static_cast<unsigned int>(Interval));
  else
    ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
}

void* GLContextGLX::GetFuncAddress(const std::string& name)
{
  return reinterpret_cast<void*>(glXGetProcAddress(reinterpret_cast<const GLubyte*>(name.c_str())));
}

void GLContextGLX::Swap()
{
  glXSwapBuffers(m_display, m_drawable);
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool GLContextGLX::Initialize(void* display_handle, void* window_handle, bool stereo, bool core)
{
  m_display = static_cast<Display*>(display_handle);
  int screen = DefaultScreen(m_display);

  // checking glx version
  int glxMajorVersion, glxMinorVersion;
  glXQueryVersion(m_display, &glxMajorVersion, &glxMinorVersion);
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
  GLXFBConfig* fbc = glXChooseFBConfig(m_display, screen, visual_attribs, &fbcount);
  if (!fbc || !fbcount)
  {
    ERROR_LOG(VIDEO, "Failed to retrieve a framebuffer config");
    return false;
  }
  m_fbconfig = *fbc;
  XFree(fbc);

  s_glxError = false;
  XErrorHandler oldHandler = XSetErrorHandler(&ctxErrorHandler);

  // Create a GLX context.
  if (core)
  {
    for (const auto& version : s_desktop_opengl_versions)
    {
      std::array<int, 9> context_attribs = {
          {GLX_CONTEXT_MAJOR_VERSION_ARB, version.first, GLX_CONTEXT_MINOR_VERSION_ARB,
           version.second, GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
           GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, None}};

      s_glxError = false;
      m_context = glXCreateContextAttribs(m_display, m_fbconfig, 0, True, &context_attribs[0]);
      XSync(m_display, False);
      m_attribs.insert(m_attribs.end(), context_attribs.begin(), context_attribs.end());
      if (!m_context || s_glxError)
        continue;

      // Got a context.
      INFO_LOG(VIDEO, "Created a GLX context with version %d.%d", version.first, version.second);
      break;
    }
  }

  // Failed to create any core contexts, try for anything.
  if (!m_context || s_glxError)
  {
    std::array<int, 5> context_attribs_legacy = {
        {GLX_CONTEXT_MAJOR_VERSION_ARB, 1, GLX_CONTEXT_MINOR_VERSION_ARB, 0, None}};
    s_glxError = false;
    m_context = glXCreateContextAttribs(m_display, m_fbconfig, 0, True, &context_attribs_legacy[0]);
    XSync(m_display, False);
    m_attribs.clear();
    m_attribs.insert(m_attribs.end(), context_attribs_legacy.begin(), context_attribs_legacy.end());
  }
  if (!m_context || s_glxError)
  {
    ERROR_LOG(VIDEO, "Unable to create GL context.");
    XSetErrorHandler(oldHandler);
    return false;
  }

  glXSwapIntervalEXTPtr = nullptr;
  glXSwapIntervalMESAPtr = nullptr;
  glXCreateGLXPbufferSGIX = nullptr;
  glXDestroyGLXPbufferSGIX = nullptr;
  m_supports_pbuffer = false;

  std::string tmp;
  std::istringstream buffer(glXQueryExtensionsString(m_display, screen));
  while (buffer >> tmp)
  {
    if (tmp == "GLX_SGIX_pbuffer")
    {
      glXCreateGLXPbufferSGIX = reinterpret_cast<PFNGLXCREATEGLXPBUFFERSGIXPROC>(
          GetFuncAddress("glXCreateGLXPbufferSGIX"));
      glXDestroyGLXPbufferSGIX = reinterpret_cast<PFNGLXDESTROYGLXPBUFFERSGIXPROC>(
          GetFuncAddress("glXDestroyGLXPbufferSGIX"));
      m_supports_pbuffer = glXCreateGLXPbufferSGIX && glXDestroyGLXPbufferSGIX;
    }
    else if (tmp == "GLX_EXT_swap_control")
    {
      glXSwapIntervalEXTPtr =
          reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(GetFuncAddress("glXSwapIntervalEXT"));
    }
    else if (tmp == "GLX_MESA_swap_control")
    {
      glXSwapIntervalMESAPtr =
          reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(GetFuncAddress("glXSwapIntervalMESA"));
    }
  }

  if (!CreateWindowSurface(reinterpret_cast<Window>(window_handle)))
  {
    ERROR_LOG(VIDEO, "Error: CreateWindowSurface failed\n");
    XSetErrorHandler(oldHandler);
    return false;
  }

  XSetErrorHandler(oldHandler);
  m_opengl_mode = Mode::OpenGL;
  return MakeCurrent();
}

std::unique_ptr<GLContext> GLContextGLX::CreateSharedContext()
{
  s_glxError = false;
  XErrorHandler oldHandler = XSetErrorHandler(&ctxErrorHandler);

  GLXContext new_glx_context =
      glXCreateContextAttribs(m_display, m_fbconfig, m_context, True, &m_attribs[0]);
  XSync(m_display, False);

  if (!new_glx_context || s_glxError)
  {
    ERROR_LOG(VIDEO, "Unable to create GL context.");
    XSetErrorHandler(oldHandler);
    return nullptr;
  }

  std::unique_ptr<GLContextGLX> new_context = std::make_unique<GLContextGLX>();
  new_context->m_context = new_glx_context;
  new_context->m_opengl_mode = m_opengl_mode;
  new_context->m_supports_pbuffer = m_supports_pbuffer;
  new_context->m_display = m_display;
  new_context->m_fbconfig = m_fbconfig;
  new_context->m_is_shared = true;

  if (m_supports_pbuffer && !new_context->CreateWindowSurface(None))
  {
    ERROR_LOG(VIDEO, "Error: CreateWindowSurface failed");
    XSetErrorHandler(oldHandler);
    return nullptr;
  }

  XSetErrorHandler(oldHandler);
  return new_context;
}

bool GLContextGLX::CreateWindowSurface(Window window_handle)
{
  if (window_handle)
  {
    // Get an appropriate visual
    XVisualInfo* vi = glXGetVisualFromFBConfig(m_display, m_fbconfig);
    m_render_window = GLX11Window::Create(m_display, window_handle, vi);
    if (!m_render_window)
      return false;

    m_backbuffer_width = m_render_window->GetWidth();
    m_backbuffer_height = m_render_window->GetHeight();
    m_drawable = static_cast<GLXDrawable>(m_render_window->GetWindow());
    XFree(vi);
  }
  else if (m_supports_pbuffer)
  {
    m_pbuffer = glXCreateGLXPbufferSGIX(m_display, m_fbconfig, 1, 1, nullptr);
    if (!m_pbuffer)
      return false;

    m_drawable = static_cast<GLXDrawable>(m_pbuffer);
  }

  return true;
}

void GLContextGLX::DestroyWindowSurface()
{
  m_render_window.reset();
  if (m_supports_pbuffer && m_pbuffer)
  {
    glXDestroyGLXPbufferSGIX(m_display, m_pbuffer);
    m_pbuffer = 0;
  }
}

bool GLContextGLX::MakeCurrent()
{
  return glXMakeCurrent(m_display, m_drawable, m_context);
}

bool GLContextGLX::ClearCurrent()
{
  return glXMakeCurrent(m_display, None, nullptr);
}

void GLContextGLX::Update()
{
  m_render_window->UpdateDimensions();
  m_backbuffer_width = m_render_window->GetWidth();
  m_backbuffer_height = m_render_window->GetHeight();
}
