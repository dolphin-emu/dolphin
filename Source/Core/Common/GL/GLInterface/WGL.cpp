// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <windows.h>
#include <array>
#include <string>

#include "Common/GL/GLInterface/WGL.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

// from wglext.h
#ifndef WGL_ARB_pbuffer
#define WGL_ARB_pbuffer 1
DECLARE_HANDLE(HPBUFFERARB);
#define WGL_DRAW_TO_PBUFFER_ARB 0x202D
#define WGL_MAX_PBUFFER_PIXELS_ARB 0x202E
#define WGL_MAX_PBUFFER_WIDTH_ARB 0x202F
#define WGL_MAX_PBUFFER_HEIGHT_ARB 0x2030
#define WGL_PBUFFER_LARGEST_ARB 0x2033
#define WGL_PBUFFER_WIDTH_ARB 0x2034
#define WGL_PBUFFER_HEIGHT_ARB 0x2035
#define WGL_PBUFFER_LOST_ARB 0x2036
typedef HPBUFFERARB(WINAPI* PFNWGLCREATEPBUFFERARBPROC)(HDC hDC, int iPixelFormat, int iWidth,
                                                        int iHeight, const int* piAttribList);
typedef HDC(WINAPI* PFNWGLGETPBUFFERDCARBPROC)(HPBUFFERARB hPbuffer);
typedef int(WINAPI* PFNWGLRELEASEPBUFFERDCARBPROC)(HPBUFFERARB hPbuffer, HDC hDC);
typedef BOOL(WINAPI* PFNWGLDESTROYPBUFFERARBPROC)(HPBUFFERARB hPbuffer);
typedef BOOL(WINAPI* PFNWGLQUERYPBUFFERARBPROC)(HPBUFFERARB hPbuffer, int iAttribute, int* piValue);
#endif /* WGL_ARB_pbuffer */

#ifndef WGL_ARB_pixel_format
#define WGL_ARB_pixel_format 1
#define WGL_NUMBER_PIXEL_FORMATS_ARB 0x2000
#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_DRAW_TO_BITMAP_ARB 0x2002
#define WGL_ACCELERATION_ARB 0x2003
#define WGL_NEED_PALETTE_ARB 0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB 0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB 0x2006
#define WGL_SWAP_METHOD_ARB 0x2007
#define WGL_NUMBER_OVERLAYS_ARB 0x2008
#define WGL_NUMBER_UNDERLAYS_ARB 0x2009
#define WGL_TRANSPARENT_ARB 0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB 0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB 0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB 0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB 0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB 0x203B
#define WGL_SHARE_DEPTH_ARB 0x200C
#define WGL_SHARE_STENCIL_ARB 0x200D
#define WGL_SHARE_ACCUM_ARB 0x200E
#define WGL_SUPPORT_GDI_ARB 0x200F
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_STEREO_ARB 0x2012
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_RED_BITS_ARB 0x2015
#define WGL_RED_SHIFT_ARB 0x2016
#define WGL_GREEN_BITS_ARB 0x2017
#define WGL_GREEN_SHIFT_ARB 0x2018
#define WGL_BLUE_BITS_ARB 0x2019
#define WGL_BLUE_SHIFT_ARB 0x201A
#define WGL_ALPHA_BITS_ARB 0x201B
#define WGL_ALPHA_SHIFT_ARB 0x201C
#define WGL_ACCUM_BITS_ARB 0x201D
#define WGL_ACCUM_RED_BITS_ARB 0x201E
#define WGL_ACCUM_GREEN_BITS_ARB 0x201F
#define WGL_ACCUM_BLUE_BITS_ARB 0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB 0x2021
#define WGL_DEPTH_BITS_ARB 0x2022
#define WGL_STENCIL_BITS_ARB 0x2023
#define WGL_AUX_BUFFERS_ARB 0x2024
#define WGL_NO_ACCELERATION_ARB 0x2025
#define WGL_GENERIC_ACCELERATION_ARB 0x2026
#define WGL_FULL_ACCELERATION_ARB 0x2027
#define WGL_SWAP_EXCHANGE_ARB 0x2028
#define WGL_SWAP_COPY_ARB 0x2029
#define WGL_SWAP_UNDEFINED_ARB 0x202A
#define WGL_TYPE_RGBA_ARB 0x202B
#define WGL_TYPE_COLORINDEX_ARB 0x202C
typedef BOOL(WINAPI* PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC hdc, int iPixelFormat,
                                                          int iLayerPlane, UINT nAttributes,
                                                          const int* piAttributes, int* piValues);
typedef BOOL(WINAPI* PFNWGLGETPIXELFORMATATTRIBFVARBPROC)(HDC hdc, int iPixelFormat,
                                                          int iLayerPlane, UINT nAttributes,
                                                          const int* piAttributes, FLOAT* pfValues);
typedef BOOL(WINAPI* PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC hdc, const int* piAttribIList,
                                                     const FLOAT* pfAttribFList, UINT nMaxFormats,
                                                     int* piFormats, UINT* nNumFormats);
#endif /* WGL_ARB_pixel_format */

#ifndef WGL_ARB_create_context
#define WGL_ARB_create_context 1
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB 0x2093
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define ERROR_INVALID_VERSION_ARB 0x2095
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext,
                                                         const int* attribList);
#endif /* WGL_ARB_create_context */

#ifndef WGL_ARB_create_context_profile
#define WGL_ARB_create_context_profile 1
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define ERROR_INVALID_PROFILE_ARB 0x2096
#endif /* WGL_ARB_create_context_profile */

#ifndef WGL_EXT_swap_control
#define WGL_EXT_swap_control 1
typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);
typedef int(WINAPI* PFNWGLGETSWAPINTERVALEXTPROC)(void);
#endif /* WGL_EXT_swap_control */

// Persistent pointers
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
static PFNWGLCREATEPBUFFERARBPROC wglCreatePbufferARB = nullptr;
static PFNWGLGETPBUFFERDCARBPROC wglGetPbufferDCARB = nullptr;
static PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDCARB = nullptr;
static PFNWGLDESTROYPBUFFERARBPROC wglDestroyPbufferARB = nullptr;

static void LoadWGLExtensions()
{
  wglSwapIntervalEXT =
      reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
  wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
      wglGetProcAddress("wglCreateContextAttribsARB"));
  wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(
      wglGetProcAddress("wglChoosePixelFormatARB"));
  wglCreatePbufferARB =
      reinterpret_cast<PFNWGLCREATEPBUFFERARBPROC>(wglGetProcAddress("wglCreatePbufferARB"));
  wglGetPbufferDCARB =
      reinterpret_cast<PFNWGLGETPBUFFERDCARBPROC>(wglGetProcAddress("wglGetPbufferDCARB"));
  wglReleasePbufferDCARB =
      reinterpret_cast<PFNWGLRELEASEPBUFFERDCARBPROC>(wglGetProcAddress("wglReleasePbufferDCARB"));
  wglDestroyPbufferARB =
      reinterpret_cast<PFNWGLDESTROYPBUFFERARBPROC>(wglGetProcAddress("wglGetPbufferDCARB"));
}

static void ClearWGLExtensionPointers()
{
  wglSwapIntervalEXT = nullptr;
  wglCreateContextAttribsARB = nullptr;
  wglChoosePixelFormatARB = nullptr;
  wglCreatePbufferARB = nullptr;
  wglGetPbufferDCARB = nullptr;
  wglReleasePbufferDCARB = nullptr;
  wglDestroyPbufferARB = nullptr;
}

GLContextWGL::~GLContextWGL()
{
  if (m_rc)
  {
    if (wglGetCurrentContext() == m_rc && !wglMakeCurrent(m_dc, nullptr))
      NOTICE_LOG(VIDEO, "Could not release drawing context.");

    if (!wglDeleteContext(m_rc))
      ERROR_LOG(VIDEO, "Attempt to release rendering context failed.");

    m_rc = nullptr;
  }

  if (m_dc)
  {
    if (m_pbuffer_handle)
    {
      wglReleasePbufferDCARB(static_cast<HPBUFFERARB>(m_pbuffer_handle), m_dc);
      m_dc = nullptr;

      wglDestroyPbufferARB(static_cast<HPBUFFERARB>(m_pbuffer_handle));
      m_pbuffer_handle = nullptr;
    }
    else
    {
      if (!ReleaseDC(m_window_handle, m_dc))
        ERROR_LOG(VIDEO, "Attempt to release device context failed.");

      m_dc = nullptr;
    }
  }
}

bool GLContextWGL::IsHeadless() const
{
  return !m_window_handle;
}

void GLContextWGL::SwapInterval(int interval)
{
  if (wglSwapIntervalEXT)
    wglSwapIntervalEXT(interval);
  else
    ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
}
void GLContextWGL::Swap()
{
  SwapBuffers(m_dc);
}

void* GLContextWGL::GetFuncAddress(const std::string& name)
{
  FARPROC func = wglGetProcAddress(name.c_str());
  if (func == nullptr)
  {
    // Using GetModuleHandle here is okay, since we import functions from opengl32.dll, it's
    // guaranteed to be loaded.
    HMODULE opengl_module = GetModuleHandle(TEXT("opengl32.dll"));
    func = GetProcAddress(opengl_module, name.c_str());
  }

  return func;
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool GLContextWGL::Initialize(void* display_handle, void* window_handle, bool core)
{
  if (!window_handle)
    return false;

  RECT window_rect = {};
  m_window_handle = reinterpret_cast<HWND>(window_handle);
  if (!GetClientRect(m_window_handle, &window_rect))
    return false;

  // Clear extension function pointers before creating the first context.
  ClearWGLExtensionPointers();

  // Control window size and picture scaling
  int twidth = window_rect.right - window_rect.left;
  int theight = window_rect.bottom - window_rect.top;
  m_backbuffer_width = twidth;
  m_backbuffer_height = theight;

  // clang-format off
  static const PIXELFORMATDESCRIPTOR pfd = {
      sizeof(PIXELFORMATDESCRIPTOR),  // Size Of This Pixel Format Descriptor
      1,                              // Version Number
      PFD_DRAW_TO_WINDOW |            // Format Must Support Window
          PFD_SUPPORT_OPENGL |        // Format Must Support OpenGL
          PFD_DOUBLEBUFFER |          // Must Support Double Buffering
          0,                // Could Support Quad Buffering
      PFD_TYPE_RGBA,                  // Request An RGBA Format
      32,                             // Select Our Color Depth
      0,
      0, 0, 0, 0, 0,   // Color Bits Ignored
      0,               // 8bit Alpha Buffer
      0,               // Shift Bit Ignored
      0,               // No Accumulation Buffer
      0, 0, 0, 0,      // Accumulation Bits Ignored
      0,               // 0Bit Z-Buffer (Depth Buffer)
      0,               // 0bit Stencil Buffer
      0,               // No Auxiliary Buffer
      PFD_MAIN_PLANE,  // Main Drawing Layer
      0,               // Reserved
      0, 0, 0          // Layer Masks Ignored
  };
  // clang-format on

  m_dc = GetDC(m_window_handle);
  if (!m_dc)
  {
    PanicAlert("(1) Can't create an OpenGL Device context. Fail.");
    return false;
  }

  int pixel_format = ChoosePixelFormat(m_dc, &pfd);
  if (!pixel_format)
  {
    PanicAlert("(2) Can't find a suitable PixelFormat.");
    return false;
  }

  if (!SetPixelFormat(m_dc, pixel_format, &pfd))
  {
    PanicAlert("(3) Can't set the PixelFormat.");
    return false;
  }

  m_rc = wglCreateContext(m_dc);
  if (!m_rc)
  {
    PanicAlert("(4) Can't create an OpenGL rendering context.");
    return false;
  }

  // WGL only supports desktop GL, for now.
  m_opengl_mode = Mode::OpenGL;

  if (core)
  {
    // Make the fallback context current, temporarily.
    // This is because we need an active context to use wglCreateContextAttribsARB.
    if (!wglMakeCurrent(m_dc, m_rc))
    {
      PanicAlert("(5) Can't make dummy WGL context current.");
      return false;
    }

    // Load WGL extension function pointers.
    LoadWGLExtensions();

    // Attempt creating a core context.
    HGLRC core_context = CreateCoreContext(m_dc, nullptr);

    // Switch out the temporary context before continuing, regardless of whether we got a core
    // context. If we didn't get a core context, the caller expects that the context is not current.
    if (!wglMakeCurrent(m_dc, nullptr))
    {
      PanicAlert("(6) Failed to switch out temporary context");
      return false;
    }

    // If we got a core context, destroy and replace the temporary context.
    if (core_context)
    {
      wglDeleteContext(m_rc);
      m_rc = core_context;
    }
    else
    {
      WARN_LOG(VIDEO, "Failed to create a core OpenGL context. Using fallback context.");
    }
  }

  return MakeCurrent();
}

std::unique_ptr<GLContext> GLContextWGL::CreateSharedContext()
{
  // WGL does not support surfaceless contexts, so we use a 1x1 pbuffer instead.
  HANDLE pbuffer;
  HDC dc;
  if (!CreatePBuffer(m_dc, 1, 1, &pbuffer, &dc))
    return nullptr;

  HGLRC rc = CreateCoreContext(dc, m_rc);
  if (!rc)
  {
    wglReleasePbufferDCARB(static_cast<HPBUFFERARB>(pbuffer), dc);
    wglDestroyPbufferARB(static_cast<HPBUFFERARB>(pbuffer));
    return nullptr;
  }

  std::unique_ptr<GLContextWGL> context = std::make_unique<GLContextWGL>();
  context->m_pbuffer_handle = pbuffer;
  context->m_dc = dc;
  context->m_rc = rc;
  context->m_opengl_mode = m_opengl_mode;
  context->m_is_shared = true;
  return context;
}

HGLRC GLContextWGL::CreateCoreContext(HDC dc, HGLRC share_context)
{
  if (!wglCreateContextAttribsARB)
  {
    WARN_LOG(VIDEO, "Missing WGL_ARB_create_context extension");
    return nullptr;
  }

  for (const auto& version : s_desktop_opengl_versions)
  {
    // Construct list of attributes. Prefer a forward-compatible, core context.
    std::array<int, 5 * 2> attribs = {WGL_CONTEXT_PROFILE_MASK_ARB,
                                      WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef _DEBUG
                                      WGL_CONTEXT_FLAGS_ARB,
                                      WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB |
                                          WGL_CONTEXT_DEBUG_BIT_ARB,
#else
                                      WGL_CONTEXT_FLAGS_ARB,
                                      WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
#endif
                                      WGL_CONTEXT_MAJOR_VERSION_ARB,
                                      version.first,
                                      WGL_CONTEXT_MINOR_VERSION_ARB,
                                      version.second,
                                      0,
                                      0};

    // Attempt creating this context.
    HGLRC core_context = wglCreateContextAttribsARB(dc, nullptr, attribs.data());
    if (core_context)
    {
      // If we're creating a shared context, share the resources before the context is used.
      if (share_context)
      {
        if (!wglShareLists(share_context, core_context))
        {
          ERROR_LOG(VIDEO, "wglShareLists failed");
          wglDeleteContext(core_context);
          return nullptr;
        }
      }

      INFO_LOG(VIDEO, "WGL: Created a GL %d.%d core context", version.first, version.second);
      return core_context;
    }
  }

  WARN_LOG(VIDEO, "Unable to create a core OpenGL context of an acceptable version");
  return nullptr;
}

bool GLContextWGL::CreatePBuffer(HDC onscreen_dc, int width, int height, HANDLE* pbuffer_handle,
                                 HDC* pbuffer_dc)
{
  if (!wglChoosePixelFormatARB || !wglCreatePbufferARB || !wglGetPbufferDCARB ||
      !wglReleasePbufferDCARB || !wglDestroyPbufferARB)
  {
    ERROR_LOG(VIDEO, "Missing WGL_ARB_pbuffer extension");
    return false;
  }

  static constexpr std::array<int, 7 * 2> pf_iattribs = {
      WGL_DRAW_TO_PBUFFER_ARB,
      1,  // Pixel format compatible with pbuffer rendering
      WGL_RED_BITS_ARB,
      0,
      WGL_GREEN_BITS_ARB,
      0,
      WGL_BLUE_BITS_ARB,
      0,
      WGL_DEPTH_BITS_ARB,
      0,
      WGL_STENCIL_BITS_ARB,
      0,
      0,
      0};

  static constexpr std::array<float, 1 * 2> pf_fattribs = {};

  int pixel_format;
  UINT num_pixel_formats;
  if (!wglChoosePixelFormatARB(onscreen_dc, pf_iattribs.data(), pf_fattribs.data(), 1,
                               &pixel_format, &num_pixel_formats))
  {
    ERROR_LOG(VIDEO, "Failed to obtain a compatible pbuffer pixel format");
    return false;
  }

  static constexpr std::array<int, 1 * 2> pb_attribs = {};

  HPBUFFERARB pbuffer =
      wglCreatePbufferARB(onscreen_dc, pixel_format, width, height, pb_attribs.data());
  if (!pbuffer)
  {
    ERROR_LOG(VIDEO, "Failed to create pbuffer");
    return false;
  }

  HDC dc = wglGetPbufferDCARB(pbuffer);
  if (!dc)
  {
    ERROR_LOG(VIDEO, "Failed to get drawing context from pbuffer");
    wglDestroyPbufferARB(pbuffer);
    return false;
  }

  *pbuffer_handle = pbuffer;
  *pbuffer_dc = dc;
  return true;
}

bool GLContextWGL::MakeCurrent()
{
  return wglMakeCurrent(m_dc, m_rc) == TRUE;
}

bool GLContextWGL::ClearCurrent()
{
  return wglMakeCurrent(m_dc, nullptr) == TRUE;
}

// Update window width, size and etc. Called from Render.cpp
void GLContextWGL::Update()
{
  RECT rcWindow;
  GetClientRect(m_window_handle, &rcWindow);

  // Get the new window width and height
  m_backbuffer_width = rcWindow.right - rcWindow.left;
  m_backbuffer_height = rcWindow.bottom - rcWindow.top;
}
