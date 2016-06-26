/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/glcanvas.cpp
// Purpose:     wxGLCanvas, for using OpenGL with wxWidgets under MS Windows
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#if wxUSE_GLCANVAS

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
#endif

#include "wx/msw/private.h"

#include "wx/glcanvas.h"

// from src/msw/window.cpp
LRESULT WXDLLEXPORT APIENTRY
wxWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#ifdef GL_EXT_vertex_array
    #define WXUNUSED_WITHOUT_GL_EXT_vertex_array(name) name
#else
    #define WXUNUSED_WITHOUT_GL_EXT_vertex_array(name) WXUNUSED(name)
#endif

// ----------------------------------------------------------------------------
// define possibly missing WGL constants and types
// ----------------------------------------------------------------------------

#ifndef WGL_ARB_pixel_format
#define WGL_ARB_pixel_format
#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_DRAW_TO_BITMAP_ARB            0x2002
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_NEED_PALETTE_ARB              0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB       0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB        0x2006
#define WGL_SWAP_METHOD_ARB               0x2007
#define WGL_NUMBER_OVERLAYS_ARB           0x2008
#define WGL_NUMBER_UNDERLAYS_ARB          0x2009
#define WGL_SUPPORT_GDI_ARB               0x200F
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_STEREO_ARB                    0x2012
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_RED_BITS_ARB                  0x2015
#define WGL_GREEN_BITS_ARB                0x2017
#define WGL_BLUE_BITS_ARB                 0x2019
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_ACCUM_BITS_ARB                0x201D
#define WGL_ACCUM_RED_BITS_ARB            0x201E
#define WGL_ACCUM_GREEN_BITS_ARB          0x201F
#define WGL_ACCUM_BLUE_BITS_ARB           0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB          0x2021
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_AUX_BUFFERS_ARB               0x2024
#define WGL_NO_ACCELERATION_ARB           0x2025
#define WGL_GENERIC_ACCELERATION_ARB      0x2026
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_SWAP_EXCHANGE_ARB             0x2028
#define WGL_SWAP_COPY_ARB                 0x2029
#define WGL_TYPE_RGBA_ARB                 0x202B
#define WGL_TYPE_COLORINDEX_ARB           0x202C
#endif

#ifndef WGL_ARB_multisample
#define WGL_ARB_multisample
#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042
#endif

#ifndef WGL_ARB_framebuffer_sRGB
#define WGL_ARB_framebuffer_sRGB
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB  0x20A9
#endif

#ifndef WGL_ARB_create_context
#define WGL_ARB_create_context
#define WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB     0x2093
#define WGL_CONTEXT_FLAGS_ARB           0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB       0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
#endif

#ifndef WGL_ARB_create_context_profile
#define WGL_ARB_create_context_profile
#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

#ifndef WGL_ARB_create_context_robustness
#define WGL_ARB_create_context_robustness
#define WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB         0x00000004
#define WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB     0x8256
#define WGL_NO_RESET_NOTIFICATION_ARB                   0x8261
#define WGL_LOSE_CONTEXT_ON_RESET_ARB                   0x8252
#endif

#ifndef WGL_ARB_robustness_application_isolation
#define WGL_ARB_robustness_application_isolation
#define WGL_CONTEXT_RESET_ISOLATION_BIT_ARB             0x00000008
#endif
#ifndef WGL_ARB_robustness_share_group_isolation
#define WGL_ARB_robustness_share_group_isolation
#endif

#ifndef WGL_ARB_context_flush_control
#define WGL_ARB_context_flush_control
#define WGL_CONTEXT_RELEASE_BEHAVIOR_ARB            0x2097
#define WGL_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB       0
#define WGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB      0x2098
#endif

#ifndef WGL_EXT_create_context_es2_profile
#define WGL_EXT_create_context_es2_profile
#define WGL_CONTEXT_ES2_PROFILE_BIT_EXT           0x00000004
#endif

#ifndef WGL_EXT_create_context_es_profile
#define WGL_EXT_create_context_es_profile
#define WGL_CONTEXT_ES_PROFILE_BIT_EXT            0x00000004
#endif

typedef HGLRC(WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)
    (HDC hDC, HGLRC hShareContext, const int *attribList);

// ----------------------------------------------------------------------------
// libraries
// ----------------------------------------------------------------------------

/*
  The following two compiler directives are specific to the Microsoft Visual
  C++ family of compilers

  Fundementally what they do is instruct the linker to use these two libraries
  for the resolution of symbols. In essence, this is the equivalent of adding
  these two libraries to either the Makefile or project file.

  This is NOT a recommended technique, and certainly is unlikely to be used
  anywhere else in wxWidgets given it is so specific to not only wxMSW, but
  also the VC compiler. However, in the case of opengl support, it's an
  applicable technique as opengl is optional in setup.h This code (wrapped by
  wxUSE_GLCANVAS), now allows opengl support to be added purely by modifying
  setup.h rather than by having to modify either the project or DSP fle.

  See MSDN for further information on the exact usage of these commands.
*/
#ifdef _MSC_VER
#  pragma comment( lib, "opengl32" )
#  pragma comment( lib, "glu32" )
#endif

//-----------------------------------------------------------------------------
// Documentation on internals
//-----------------------------------------------------------------------------

//  OpenGL has evolved not only adding new features, but also there are new
//  ways of doing things. Among these new ways, we pay special attention to
//  pixel format choosing and context creation.
//
//  The old way of choosing a pixel format is to use a PIXELFORMATDESCRIPTOR
//  struct for setting the wanted attributes and to call ChoosePixelFormat()
//  with that struct.
//  When new attributes came into scene, the MSW struct became inadequate, and
//  a new method was implemented: wglChoosePixelFormatARB().
//
//  For rendering context creation wglCreateContext() was the function to call.
//  Starting with OpenGL 3.0 (2009) there are several attributes for context,
//  specially for wanted version and "core" or "compatibility" profiles.
//  In order to use these new features, wglCreateContextAttribsARB() must be
//  called instead of wglCreateContext().
//
//  wxWidgets handles this OpenGL evolution trying to use the new ways, and
//  falling back to the old ways if neither the latter are available nor the
//  user requires them.
//
//  wxGLAttributes is used for pixel format attributes.
//  wxGLContextAttrs is used for OpenGL rendering context attributes.
//  wxWidgets does not handle all of OpenGL attributes. This is because some of
//  them are platform-dependent, or perhaps too new for wx. To cope with these
//  cases, these two objects allow the user to set his own attributes.
//
//  To keep wxWidgets backwards compatibility, a list of mixed attributes is
//  still allowed at wxGLCanvas constructor.


// ----------------------------------------------------------------------------
// wxGLContextAttrs: OpenGL rendering context attributes
// ----------------------------------------------------------------------------
// MSW specific values

wxGLContextAttrs& wxGLContextAttrs::CoreProfile()
{
    AddAttribBits(WGL_CONTEXT_PROFILE_MASK_ARB,
                  WGL_CONTEXT_CORE_PROFILE_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::MajorVersion(int val)
{
    if ( val > 0 )
    {
        AddAttribute(WGL_CONTEXT_MAJOR_VERSION_ARB);
        AddAttribute(val);
        if ( val >= 3 )
            SetNeedsARB();
    }
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::MinorVersion(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(WGL_CONTEXT_MINOR_VERSION_ARB);
        AddAttribute(val);
    }
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::CompatibilityProfile()
{
    AddAttribBits(WGL_CONTEXT_PROFILE_MASK_ARB,
                  WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::ForwardCompatible()
{
    AddAttribBits(WGL_CONTEXT_FLAGS_ARB,
                  WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::ES2()
{
    AddAttribBits(WGL_CONTEXT_PROFILE_MASK_ARB,
                  WGL_CONTEXT_ES2_PROFILE_BIT_EXT);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::DebugCtx()
{
    AddAttribBits(WGL_CONTEXT_FLAGS_ARB,
                  WGL_CONTEXT_DEBUG_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::Robust()
{
    AddAttribBits(WGL_CONTEXT_FLAGS_ARB,
                  WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::NoResetNotify()
{
    AddAttribute(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
    AddAttribute(WGL_NO_RESET_NOTIFICATION_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::LoseOnReset()
{
    AddAttribute(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
    AddAttribute(WGL_LOSE_CONTEXT_ON_RESET_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::ResetIsolation()
{
    AddAttribBits(WGL_CONTEXT_FLAGS_ARB,
                  WGL_CONTEXT_RESET_ISOLATION_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::ReleaseFlush(int val)
{
    AddAttribute(WGL_CONTEXT_RELEASE_BEHAVIOR_ARB);
    if ( val == 1 )
        AddAttribute(WGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB);
    else
        AddAttribute(WGL_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::PlatformDefaults()
{
    // No MSW specific defaults
    return *this;
}

void wxGLContextAttrs::EndList()
{
    AddAttribute(0);
}

// ----------------------------------------------------------------------------
// wxGLAttributes: pixel format attributes
// ----------------------------------------------------------------------------
// MSW specific values

wxGLAttributes& wxGLAttributes::RGBA()
{
    AddAttribute(WGL_PIXEL_TYPE_ARB);
    AddAttribute(WGL_TYPE_RGBA_ARB);
    AddAttribute(WGL_COLOR_BITS_ARB);
    AddAttribute(24);
    AddAttribute(WGL_ALPHA_BITS_ARB);
    AddAttribute(8);
    return *this;
}

wxGLAttributes& wxGLAttributes::BufferSize(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(WGL_COLOR_BITS_ARB);
        AddAttribute(val);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::Level(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(WGL_NUMBER_OVERLAYS_ARB);
        AddAttribute(val);
    }
    else if ( val < 0 )
    {
        AddAttribute(WGL_NUMBER_UNDERLAYS_ARB);
        AddAttribute(-val);
    }
    if ( val < -1 || val > 1 )
    {
        SetNeedsARB();
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::DoubleBuffer()
{
    AddAttribute(WGL_DOUBLE_BUFFER_ARB);
    AddAttribute(GL_TRUE);
    return *this;
}

wxGLAttributes& wxGLAttributes::Stereo()
{
    AddAttribute(WGL_STEREO_ARB);
    AddAttribute(GL_TRUE);
    return *this;
}

wxGLAttributes& wxGLAttributes::AuxBuffers(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(WGL_AUX_BUFFERS_ARB);
        AddAttribute(val);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::MinRGBA(int mRed, int mGreen, int mBlue, int mAlpha)
{
    int colorBits = 0;
    if ( mRed >= 0)
    {
        AddAttribute(WGL_RED_BITS_ARB);
        AddAttribute(mRed);
        colorBits += mRed;
    }
    if ( mGreen >= 0)
    {
        AddAttribute(WGL_GREEN_BITS_ARB);
        AddAttribute(mGreen);
        colorBits += mGreen;
    }
    if ( mBlue >= 0)
    {
        AddAttribute(WGL_BLUE_BITS_ARB);
        AddAttribute(mBlue);
        colorBits += mBlue;
    }
    if ( mAlpha >= 0)
    {
        AddAttribute(WGL_ALPHA_BITS_ARB);
        AddAttribute(mAlpha);
        // doesn't count in colorBits
    }
    if ( colorBits )
    {
        AddAttribute(WGL_COLOR_BITS_ARB);
        AddAttribute(colorBits);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::Depth(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(WGL_DEPTH_BITS_ARB);
        AddAttribute(val);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::Stencil(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(WGL_STENCIL_BITS_ARB);
        AddAttribute(val);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::MinAcumRGBA(int mRed, int mGreen, int mBlue, int mAlpha)
{
    int acumBits = 0;
    if ( mRed >= 0)
    {
        AddAttribute(WGL_ACCUM_RED_BITS_ARB);
        AddAttribute(mRed);
        acumBits += mRed;
    }
    if ( mGreen >= 0)
    {
        AddAttribute(WGL_ACCUM_GREEN_BITS_ARB);
        AddAttribute(mGreen);
        acumBits += mGreen;
    }
    if ( mBlue >= 0)
    {
        AddAttribute(WGL_ACCUM_BLUE_BITS_ARB);
        AddAttribute(mBlue);
        acumBits += mBlue;
    }
    if ( mAlpha >= 0)
    {
        AddAttribute(WGL_ACCUM_ALPHA_BITS_ARB);
        AddAttribute(mAlpha);
        acumBits += mAlpha;
    }
    if ( acumBits )
    {
        AddAttribute(WGL_ACCUM_BITS_ARB);
        AddAttribute(acumBits);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::SampleBuffers(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(WGL_SAMPLE_BUFFERS_ARB);
        AddAttribute(val);
        SetNeedsARB();
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::Samplers(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(WGL_SAMPLES_ARB);
        AddAttribute(val);
        SetNeedsARB();
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::FrameBuffersRGB()
{
    AddAttribute(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB);
    AddAttribute(GL_TRUE);
    SetNeedsARB();
    return *this;
}

void wxGLAttributes::EndList()
{
    AddAttribute(0);
}

wxGLAttributes& wxGLAttributes::PlatformDefaults()
{
    AddAttribute(WGL_DRAW_TO_WINDOW_ARB);
    AddAttribute(GL_TRUE);
    AddAttribute(WGL_SUPPORT_OPENGL_ARB);
    AddAttribute(GL_TRUE);
    AddAttribute(WGL_ACCELERATION_ARB);
    AddAttribute(WGL_FULL_ACCELERATION_ARB);
    return *this;
}

wxGLAttributes& wxGLAttributes::Defaults()
{
    RGBA().Depth(16).DoubleBuffer().SampleBuffers(1).Samplers(4);
    SetNeedsARB();
    return *this;
}

// ----------------------------------------------------------------------------
// wxGLContext
// ----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGLContext, wxObject);

wxGLContext::wxGLContext(wxGLCanvas *win,
                         const wxGLContext *other,
                         const wxGLContextAttrs *ctxAttrs)
    : m_glContext(NULL)
{
    const int* contextAttribs = NULL;
    bool needsARB = false;

    if ( ctxAttrs )
    {
        contextAttribs = ctxAttrs->GetGLAttrs();
        needsARB = ctxAttrs->NeedsARB();
    }
    else if ( win->GetGLCTXAttrs().GetGLAttrs() )
    {
        // If OpenGL context parameters were set at wxGLCanvas ctor, get them now
        contextAttribs = win->GetGLCTXAttrs().GetGLAttrs();
        needsARB = win->GetGLCTXAttrs().NeedsARB();
    }
    // else use GPU driver defaults

    m_isOk = false;

    // We need to create a temporary context to get the pointer to
    // wglCreateContextAttribsARB function
    HGLRC tempContext = wglCreateContext(win->GetHDC());
    wxCHECK_RET( tempContext, "wglCreateContext failed!" );

    wglMakeCurrent(win->GetHDC(), tempContext);
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB
            = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
            wglGetProcAddress("wglCreateContextAttribsARB");
    wglMakeCurrent(win->GetHDC(), NULL);
    wglDeleteContext(tempContext);

    // The preferred way is using wglCreateContextAttribsARB, even for old context
    if ( !wglCreateContextAttribsARB && needsARB ) // OpenGL 3 context creation
    {
        wxLogMessage(_("OpenGL 3.0 or later is not supported by the OpenGL driver."));
        return;
    }

    if ( wglCreateContextAttribsARB )
    {
        m_glContext = wglCreateContextAttribsARB(win->GetHDC(),
                                                other ? other->m_glContext : 0,
                                                contextAttribs);
    }
    else
    {
        // Create legacy context
        m_glContext = wglCreateContext(win->GetHDC());
        // Set shared context
        if ( other && !wglShareLists(other->m_glContext, m_glContext) )
            wxLogLastError("wglShareLists");
     }

    if ( !m_glContext )
        wxLogMessage(_("Couldn't create OpenGL context"));
    else
        m_isOk = true;
}

wxGLContext::~wxGLContext()
{
    // note that it's ok to delete the context even if it's the current one
    if ( m_glContext )
    {
        wglDeleteContext(m_glContext);
    }
}

bool wxGLContext::SetCurrent(const wxGLCanvas& win) const
{
    if ( !wglMakeCurrent(win.GetHDC(), m_glContext) )
    {
        wxLogLastError(wxT("wglMakeCurrent"));
        return false;
    }
    return true;
}

// ============================================================================
// wxGLCanvas
// ============================================================================

wxIMPLEMENT_CLASS(wxGLCanvas, wxWindow);

wxBEGIN_EVENT_TABLE(wxGLCanvas, wxWindow)
#if wxUSE_PALETTE
    EVT_PALETTE_CHANGED(wxGLCanvas::OnPaletteChanged)
    EVT_QUERY_NEW_PALETTE(wxGLCanvas::OnQueryNewPalette)
#endif
wxEND_EVENT_TABLE()

// ----------------------------------------------------------------------------
// wxGLCanvas construction
// ----------------------------------------------------------------------------

void wxGLCanvas::Init()
{
#if WXWIN_COMPATIBILITY_2_8
    m_glContext = NULL;
#endif
    m_hDC = NULL;
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       const wxGLAttributes& dispAttrs,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const wxPalette& palette)
{
    Init();

    (void)Create(parent, dispAttrs, id, pos, size, style, name, palette);
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       wxWindowID id,
                       const int *attribList,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const wxPalette& palette)
{
    Init();

    (void)Create(parent, id, pos, size, style, name, attribList, palette);
}

wxGLCanvas::~wxGLCanvas()
{
    ::ReleaseDC(GetHwnd(), m_hDC);
}

// Replaces wxWindow::Create functionality, since we need to use a different
// window class
bool wxGLCanvas::CreateWindow(wxWindow *parent,
                              wxWindowID id,
                              const wxPoint& pos,
                              const wxSize& size,
                              long style,
                              const wxString& name)
{
    wxCHECK_MSG( parent, false, wxT("can't create wxWindow without parent") );

    if ( !CreateBase(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    parent->AddChild(this);

    /*
       A general rule with OpenGL and Win32 is that any window that will have a
       HGLRC built for it must have two flags:  WS_CLIPCHILDREN & WS_CLIPSIBLINGS.
       You can find references about this within the knowledge base and most OpenGL
       books that contain the wgl function descriptions.
     */
    WXDWORD exStyle = 0;
    DWORD msflags = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    msflags |= MSWGetStyle(style, &exStyle);

    if ( !MSWCreate(wxApp::GetRegisteredClassName(wxT("wxGLCanvas"), -1, CS_OWNDC),
                    NULL, pos, size, msflags, exStyle) )
        return false;

    m_hDC = ::GetDC(GetHwnd());
    if ( !m_hDC )
        return false;

    return true;
}

bool wxGLCanvas::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name,
                        const int *attribList,
                        const wxPalette& palette)
{
    // Separate 'pixel format' attributes.
    // Also store context attributes for wxGLContext ctor
    wxGLAttributes dispAttrs;
    if ( ! ParseAttribList(attribList, dispAttrs, &m_GLCTXAttrs) )
        return false;

    return Create(parent, dispAttrs, id, pos, size, style, name, palette);
}

bool wxGLCanvas::Create(wxWindow *parent,
                        const wxGLAttributes& dispAttrs,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name,
                        const wxPalette& palette)
{
    // Create the window first: we will either use it as is or use it to query
    // for multisampling support and recreate it later with another pixel format
    if ( !CreateWindow(parent, id, pos, size, style, name) )
        return false;

    // Choose a matching pixel format.
    // Need a PIXELFORMATDESCRIPTOR for SetPixelFormat()
    PIXELFORMATDESCRIPTOR pfd;
    int pixelFormat = FindMatchingPixelFormat(dispAttrs, &pfd);
    if ( !pixelFormat )
    {
        wxFAIL_MSG("Can't find a pixel format for the requested attributes");
        return false;
    }

    // From SetPixelFormat() docs, relating pfd parameter:
    // https://msdn.microsoft.com/en-us/library/dd369049%28v=vs.85%29.aspx
    //   "The system's metafile component uses this structure to record the
    //   logical pixel format specification."
    // If anybody understands this sentence, please explain.
    // Pass pfd just in case it's somehow needed. Passing NULL also works here.
    if ( !::SetPixelFormat(m_hDC, pixelFormat, &pfd) )
    {
        wxLogLastError("SetPixelFormat");
        return false;
    }

#if wxUSE_PALETTE
    if ( !SetupPalette(palette) )
        return false;
#else // !wxUSE_PALETTE
    wxUnusedVar(palette);
#endif // wxUSE_PALETTE/!wxUSE_PALETTE

    return true;
}

// ----------------------------------------------------------------------------
// operations
// ----------------------------------------------------------------------------

bool wxGLCanvas::SwapBuffers()
{
    if ( !::SwapBuffers(m_hDC) )
    {
        wxLogLastError(wxT("SwapBuffers"));
        return false;
    }

    return true;
}


// this macro defines a variable of type "name_t" called "name" and initializes
// it with the pointer to WGL function "name" (which may be NULL)
#define wxDEFINE_WGL_FUNC(name) \
    name##_t name = (name##_t)wglGetProcAddress(#name)

/* static */
bool wxGLCanvasBase::IsExtensionSupported(const char *extension)
{
    static const char *s_extensionsList = (char *)wxUIntPtr(-1);
    if ( s_extensionsList == (char *)wxUIntPtr(-1) )
    {
        typedef const char * (WINAPI *wglGetExtensionsStringARB_t)(HDC hdc);

        wxDEFINE_WGL_FUNC(wglGetExtensionsStringARB);
        if ( wglGetExtensionsStringARB )
        {
            s_extensionsList = wglGetExtensionsStringARB(wglGetCurrentDC());
        }
        else
        {
            typedef const char * (WINAPI * wglGetExtensionsStringEXT_t)();

            wxDEFINE_WGL_FUNC(wglGetExtensionsStringEXT);
            if ( wglGetExtensionsStringEXT )
            {
                s_extensionsList = wglGetExtensionsStringEXT();
            }
            else
            {
                s_extensionsList = NULL;
            }
        }
    }

    return s_extensionsList && IsExtensionInList(s_extensionsList, extension);
}

// ----------------------------------------------------------------------------
// pixel format stuff
// ----------------------------------------------------------------------------

// A dummy window, needed at FindMatchingPixelFormat()
class WXDLLIMPEXP_GL wxGLdummyWin : public wxWindow
{
public:
    wxGLdummyWin()
    {
        hdc = 0;
        CreateBase(NULL, wxID_ANY);
        DWORD msflags = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        if( MSWCreate(wxApp::GetRegisteredClassName(wxT("wxGLCanvas"), -1, CS_OWNDC),
                      NULL, wxDefaultPosition, wxDefaultSize, msflags, 0) )
        {
            hdc = ::GetDC(GetHwnd());
        }
    }
    ~wxGLdummyWin()
    {
        if ( hdc )
            ::ReleaseDC(GetHwnd(), hdc);
    }
    HDC hdc;
};

// Fills PIXELFORMATDESCRIPTOR struct
static void SetPFDForAttributes(PIXELFORMATDESCRIPTOR& pfd, const int* attrsListWGL)
{
    // Some defaults
    pfd.nSize =  sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.iLayerType = PFD_MAIN_PLANE; // For very early MSW OpenGL

    // We can meet some WGL_XX values not managed by wx. But the user
    // may require them. Allow here those that are also used for pfd.
    // Color shift and transparency are not handled.
    for ( int arg = 0; attrsListWGL[arg]; )
    {
        switch ( attrsListWGL[arg++] )
        {
            case WGL_DRAW_TO_WINDOW_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_DRAW_TO_WINDOW;
                break;

            case WGL_DRAW_TO_BITMAP_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_DRAW_TO_BITMAP;
                break;

            case WGL_ACCELERATION_ARB:
                if ( attrsListWGL[arg++] == WGL_GENERIC_ACCELERATION_ARB )
                    pfd.dwFlags |= PFD_GENERIC_ACCELERATED;
                break;

            case WGL_NEED_PALETTE_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_NEED_PALETTE;
                break;

            case WGL_NEED_SYSTEM_PALETTE_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_NEED_SYSTEM_PALETTE;
                break;

            case WGL_SWAP_LAYER_BUFFERS_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_SWAP_LAYER_BUFFERS;
                break;

            case WGL_SWAP_METHOD_ARB:
                if ( attrsListWGL[arg++] == WGL_SWAP_EXCHANGE_ARB )
                    pfd.dwFlags |= PFD_SWAP_EXCHANGE;
                else if ( attrsListWGL[arg] == WGL_SWAP_COPY_ARB )
                    pfd.dwFlags |= PFD_SWAP_COPY;
                break;

            case WGL_NUMBER_OVERLAYS_ARB:
                pfd.bReserved &= 240;
                pfd.bReserved |= attrsListWGL[arg++] & 15;
                break;

            case WGL_NUMBER_UNDERLAYS_ARB:
                pfd.bReserved &= 15;
                pfd.bReserved |= attrsListWGL[arg++] & 240;
                break;

            case WGL_SUPPORT_GDI_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_SUPPORT_GDI;
                break;

            case WGL_SUPPORT_OPENGL_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_SUPPORT_OPENGL;
                break;

            case WGL_DOUBLE_BUFFER_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_DOUBLEBUFFER;
                break;

            case WGL_STEREO_ARB:
                if ( attrsListWGL[arg++] )
                    pfd.dwFlags |= PFD_STEREO;
                break;

            case WGL_PIXEL_TYPE_ARB:
                if ( attrsListWGL[arg++] == WGL_TYPE_RGBA_ARB )
                    pfd.iPixelType = PFD_TYPE_RGBA;
                else
                    pfd.iPixelType = PFD_TYPE_COLORINDEX;
                break;

            case WGL_COLOR_BITS_ARB:
                pfd.cColorBits = attrsListWGL[arg++];
                break;

            case WGL_RED_BITS_ARB:
                pfd.cRedBits = attrsListWGL[arg++];
                break;

            case WGL_GREEN_BITS_ARB:
                pfd.cGreenBits = attrsListWGL[arg++];
                break;

            case WGL_BLUE_BITS_ARB:
                pfd.cBlueBits = attrsListWGL[arg++];
                break;

            case WGL_ALPHA_BITS_ARB:
                pfd.cAlphaBits = attrsListWGL[arg++];
                break;

            case WGL_ACCUM_BITS_ARB:
                pfd.cAccumBits = attrsListWGL[arg++];
                break;

            case WGL_ACCUM_RED_BITS_ARB:
                pfd.cAccumRedBits = attrsListWGL[arg++];
                break;

            case WGL_ACCUM_GREEN_BITS_ARB:
                pfd.cAccumGreenBits = attrsListWGL[arg++];
                break;

            case WGL_ACCUM_BLUE_BITS_ARB:
                pfd.cAccumBlueBits = attrsListWGL[arg++];
                break;

            case WGL_ACCUM_ALPHA_BITS_ARB:
                pfd.cAccumAlphaBits = attrsListWGL[arg++];
                break;

            case WGL_DEPTH_BITS_ARB:
                pfd.cDepthBits = attrsListWGL[arg++];
                break;

            case WGL_STENCIL_BITS_ARB:
                pfd.cStencilBits = attrsListWGL[arg++];
                break;

            case WGL_AUX_BUFFERS_ARB:
                pfd.cAuxBuffers = attrsListWGL[arg++];
                break;

            default:
                // ignore
                break;
        }
    }
}

/* static */
int wxGLCanvas::FindMatchingPixelFormat(const wxGLAttributes& dispAttrs,
                                        PIXELFORMATDESCRIPTOR* ppfd)
{
    // WGL_XX attributes
    const int* attrsListWGL = dispAttrs.GetGLAttrs();
    if ( !attrsListWGL )
    {
        wxFAIL_MSG("wxGLAttributes object is empty.");
        return 0;
    }

    // The preferred way is using wglChoosePixelFormatARB. This is not a MSW
    // function, we must ask the GPU driver for a pointer to it. We need a
    // rendering context for this operation. Create a dummy one.
    // Notice that we don't create a wxGLContext on purpose.
    // We meet another issue:
    // Before creating any context, we must set a pixel format for its hdc:
    //   https://msdn.microsoft.com/en-us/library/dd374379%28v=vs.85%29.aspx
    // but we can't set a pixel format more than once:
    //   https://msdn.microsoft.com/en-us/library/dd369049%28v=vs.85%29.aspx
    // To cope with this we need a dummy hidden window.
    //
    // Having this dummy window allows also calling IsDisplaySupported()
    // without creating a wxGLCanvas.
    wxGLdummyWin* dummyWin = new wxGLdummyWin();
    HDC dummyHDC = dummyWin->hdc;
    if ( !dummyHDC )
    {
        dummyWin->Destroy();
        wxFAIL_MSG("Can't create dummy window");
        return 0;
    }
    // Dummy context
    PIXELFORMATDESCRIPTOR dpfd; //any one is valid
    ::SetPixelFormat(dummyHDC, 1, &dpfd); // pixelformat=1, any one is valid
    HGLRC dumctx = ::wglCreateContext(dummyHDC);
    if ( !dumctx )
    {
        dummyWin->Destroy();
        // A fatal error!
        wxFAIL_MSG("wglCreateContext failed!");
        return 0;
    }

    ::wglMakeCurrent(dummyHDC, dumctx);

    typedef BOOL (WINAPI * wglChoosePixelFormatARB_t)
                 (HDC hdc,
                  const int *piAttribIList,
                  const FLOAT *pfAttribFList,
                  UINT nMaxFormats,
                  int *piFormats,
                  UINT *nNumFormats
                 );

    wxDEFINE_WGL_FUNC(wglChoosePixelFormatARB); // get a pointer to it

    // If wglChoosePixelFormatARB is not supported but  the attributes require
    // it, then fail.
    if ( !wglChoosePixelFormatARB && dispAttrs.NeedsARB() )
    {
        wxLogLastError("wglChoosePixelFormatARB unavailable");
        // Delete the dummy objects
        ::wglMakeCurrent(NULL, NULL);
        ::wglDeleteContext(dumctx);
        dummyWin->Destroy();
        return 0;
    }

    int pixelFormat = 0;

    if ( !wglChoosePixelFormatARB && !dispAttrs.NeedsARB() )
    {
        // Old way
        if ( !ppfd )
        {
            // We have been called from IsDisplaySupported()
            PIXELFORMATDESCRIPTOR pfd;
            SetPFDForAttributes(pfd, attrsListWGL);
            pixelFormat = ::ChoosePixelFormat(dummyHDC, &pfd);
        }
        else
        {
            SetPFDForAttributes(*ppfd, attrsListWGL);
            pixelFormat = ::ChoosePixelFormat(dummyHDC, ppfd);
        }
        // We should ensure that we have got what we have asked for. This can
        // be done using DescribePixelFormat() and comparing attributes.
        // Nevertheless wglChoosePixelFormatARB exists since 2001, so it's
        // very unlikely that ChoosePixelFormat() is used. So, do nothing.
    }
    else
    {
        // New way, using wglChoosePixelFormatARB
        // 'ppfd' is used at wxGLCanvas::Create(). See explanations there.
        if ( ppfd )
            SetPFDForAttributes(*ppfd, attrsListWGL);

        UINT numFormats = 0;

        // Get the first good match
        if ( !wglChoosePixelFormatARB(dummyHDC, attrsListWGL, NULL,
                                      1, &pixelFormat, &numFormats) )
        {
            wxLogLastError("wglChoosePixelFormatARB. Is the list zero-terminated?");
            numFormats = 0;
        }

        // Although TRUE is returned if no matching formats are found (see
        // https://www.opengl.org/registry/specs/ARB/wgl_pixel_format.txt),
        // pixelFormat is not initialized in this case so we need to check
        // for numFormats being not 0 explicitly (however this is not an error
        // so don't call wxLogLastError() here).
        if ( !numFormats )
            pixelFormat = 0;
    }

    // Delete the dummy objects
    ::wglMakeCurrent(NULL, NULL);
    ::wglDeleteContext(dumctx);
    dummyWin->Destroy();

    return pixelFormat;
}

/* static */
int
wxGLCanvas::ChooseMatchingPixelFormat(HDC hdc,
                                      const int *attribList,
                                      PIXELFORMATDESCRIPTOR *ppfd)
{
    wxGLAttributes dispAttrs;
    ParseAttribList(attribList, dispAttrs);
    wxUnusedVar(hdc);
    return FindMatchingPixelFormat(dispAttrs, ppfd);
}

/* static */
bool wxGLCanvasBase::IsDisplaySupported(const wxGLAttributes& dispAttrs)
{
    return wxGLCanvas::FindMatchingPixelFormat(dispAttrs) > 0;
}

/* static */
bool wxGLCanvasBase::IsDisplaySupported(const int *attribList)
{
    // We need a device context to test the pixel format, so get one
    // for the root window.
    // Not true anymore. Keep it just in case some body uses this undocumented function
    return wxGLCanvas::ChooseMatchingPixelFormat(ScreenHDC(), attribList) > 0;
}

int wxGLCanvas::DoSetup(PIXELFORMATDESCRIPTOR &pfd, const int *attribList)
{
    // Keep this member is case somebody is overriding it
    wxUnusedVar(pfd);
    wxUnusedVar(attribList);
    return -1;
}

// ----------------------------------------------------------------------------
// palette stuff
// ----------------------------------------------------------------------------

#if wxUSE_PALETTE

bool wxGLCanvas::SetupPalette(const wxPalette& palette)
{
    const int pixelFormat = ::GetPixelFormat(m_hDC);
    if ( !pixelFormat )
    {
        wxLogLastError(wxT("GetPixelFormat"));
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd;
    if ( !::DescribePixelFormat(m_hDC, pixelFormat, sizeof(pfd), &pfd) )
    {
        wxLogLastError(wxT("DescribePixelFormat"));
        return false;
    }

    if ( !(pfd.dwFlags & PFD_NEED_PALETTE) )
        return true;

    m_palette = palette;

    if ( !m_palette.IsOk() )
    {
        m_palette = CreateDefaultPalette();
        if ( !m_palette.IsOk() )
            return false;
    }

    if ( !::SelectPalette(m_hDC, GetHpaletteOf(m_palette), FALSE) )
    {
        wxLogLastError(wxT("SelectPalette"));
        return false;
    }

    if ( ::RealizePalette(m_hDC) == GDI_ERROR )
    {
        wxLogLastError(wxT("RealizePalette"));
        return false;
    }

    return true;
}

wxPalette wxGLCanvas::CreateDefaultPalette()
{
    PIXELFORMATDESCRIPTOR pfd;
    int paletteSize;
    int pixelFormat = GetPixelFormat(m_hDC);

    DescribePixelFormat(m_hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    paletteSize = 1 << pfd.cColorBits;

    LOGPALETTE* pPal =
     (LOGPALETTE*) malloc(sizeof(LOGPALETTE) + paletteSize * sizeof(PALETTEENTRY));
    pPal->palVersion = 0x300;
    pPal->palNumEntries = (WORD)paletteSize;

    /* build a simple RGB color palette */
    int redMask = (1 << pfd.cRedBits) - 1;
    int greenMask = (1 << pfd.cGreenBits) - 1;
    int blueMask = (1 << pfd.cBlueBits) - 1;

    for (int i=0; i<paletteSize; ++i)
    {
        pPal->palPalEntry[i].peRed =
            (BYTE)((((i >> pfd.cRedShift) & redMask) * 255) / redMask);
        pPal->palPalEntry[i].peGreen =
            (BYTE)((((i >> pfd.cGreenShift) & greenMask) * 255) / greenMask);
        pPal->palPalEntry[i].peBlue =
            (BYTE)((((i >> pfd.cBlueShift) & blueMask) * 255) / blueMask);
        pPal->palPalEntry[i].peFlags = 0;
    }

    HPALETTE hPalette = CreatePalette(pPal);
    free(pPal);

    wxPalette palette;
    palette.SetHPALETTE((WXHPALETTE) hPalette);

    return palette;
}

void wxGLCanvas::OnQueryNewPalette(wxQueryNewPaletteEvent& event)
{
  /* realize palette if this is the current window */
  if ( GetPalette()->IsOk() ) {
    ::UnrealizeObject((HPALETTE) GetPalette()->GetHPALETTE());
    ::SelectPalette(GetHDC(), (HPALETTE) GetPalette()->GetHPALETTE(), FALSE);
    ::RealizePalette(GetHDC());
    Refresh();
    event.SetPaletteRealized(true);
  }
  else
    event.SetPaletteRealized(false);
}

void wxGLCanvas::OnPaletteChanged(wxPaletteChangedEvent& event)
{
  /* realize palette if this is *not* the current window */
  if ( GetPalette() &&
       GetPalette()->IsOk() && (this != event.GetChangedWindow()) )
  {
    ::UnrealizeObject((HPALETTE) GetPalette()->GetHPALETTE());
    ::SelectPalette(GetHDC(), (HPALETTE) GetPalette()->GetHPALETTE(), FALSE);
    ::RealizePalette(GetHDC());
    Refresh();
  }
}

#endif // wxUSE_PALETTE

// ----------------------------------------------------------------------------
// deprecated wxGLCanvas methods using implicit wxGLContext
// ----------------------------------------------------------------------------

// deprecated constructors creating an implicit m_glContext
#if WXWIN_COMPATIBILITY_2_8

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const int *attribList,
                       const wxPalette& palette)
{
    Init();

    if ( Create(parent, id, pos, size, style, name, attribList, palette) )
        m_glContext = new wxGLContext(this);
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       const wxGLContext *shared,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const int *attribList,
                       const wxPalette& palette)
{
    Init();

    if ( Create(parent, id, pos, size, style, name, attribList, palette) )
        m_glContext = new wxGLContext(this, shared);
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       const wxGLCanvas *shared,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const int *attribList,
                       const wxPalette& palette)
{
    Init();

    if ( Create(parent, id, pos, size, style, name, attribList, palette) )
        m_glContext = new wxGLContext(this, shared ? shared->m_glContext : NULL);
}

#endif // WXWIN_COMPATIBILITY_2_8


// ----------------------------------------------------------------------------
// wxGLApp
// ----------------------------------------------------------------------------

bool wxGLApp::InitGLVisual(const int *attribList)
{
    if ( !wxGLCanvas::ChooseMatchingPixelFormat(ScreenHDC(), attribList) )
    {
        wxLogError(_("Failed to initialize OpenGL"));
        return false;
    }

    return true;
}

#endif // wxUSE_GLCANVAS
