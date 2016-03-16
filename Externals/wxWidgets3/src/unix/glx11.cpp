///////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/glx11.cpp
// Purpose:     code common to all X11-based wxGLCanvas implementations
// Author:      Vadim Zeitlin
// Created:     2007-04-15
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_GLCANVAS

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif //WX_PRECOMP

#include "wx/glcanvas.h"

// IRIX headers call this differently
#ifdef __SGI__
    #ifndef GLX_SAMPLE_BUFFERS_ARB
        #define GLX_SAMPLE_BUFFERS_ARB GLX_SAMPLE_BUFFERS_SGIS
    #endif
    #ifndef GLX_SAMPLES_ARB
        #define GLX_SAMPLES_ARB GLX_SAMPLES_SGIS
    #endif
#endif // __SGI__

// ----------------------------------------------------------------------------
// define possibly missing XGL constants and types
// ----------------------------------------------------------------------------

#ifndef GLX_NONE_EXT
#define GLX_NONE_EXT                       0x8000
#endif

#ifndef GLX_ARB_multisample
#define GLX_ARB_multisample
#define GLX_SAMPLE_BUFFERS_ARB             100000
#define GLX_SAMPLES_ARB                    100001
#endif

#ifndef GLX_EXT_visual_rating
#define GLX_EXT_visual_rating
#define GLX_VISUAL_CAVEAT_EXT              0x20
#define GLX_NONE_EXT                       0x8000
#define GLX_SLOW_VISUAL_EXT                0x8001
#define GLX_NON_CONFORMANT_VISUAL_EXT      0x800D
#endif

#ifndef GLX_EXT_visual_info
#define GLX_EXT_visual_info
#define GLX_X_VISUAL_TYPE_EXT              0x22
#define GLX_DIRECT_COLOR_EXT               0x8003
#endif

#ifndef GLX_ARB_framebuffer_sRGB
#define GLX_ARB_framebuffer_sRGB
#define GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB   0x20B2
#endif

/* Typedef for the GL 3.0 context creation function */
typedef GLXContext(*PFNGLXCREATECONTEXTATTRIBSARBPROC)
    (Display * dpy, GLXFBConfig config, GLXContext share_context,
    Bool direct, const int *attrib_list);

#ifndef GLX_ARB_create_context
#define GLX_ARB_create_context
#define GLX_CONTEXT_MAJOR_VERSION_ARB      0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB      0x2092
#define GLX_CONTEXT_FLAGS_ARB              0x2094
#define GLX_CONTEXT_DEBUG_BIT_ARB          0x0001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#endif

#ifndef GLX_ARB_create_context_profile
#define GLX_ARB_create_context_profile
#define GLX_CONTEXT_PROFILE_MASK_ARB       0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB   0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

#ifndef GLX_ARB_create_context_robustness
#define GLX_ARB_create_context_robustness
#define GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB  0x00000004
#define GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB     0x8256
#define GLX_NO_RESET_NOTIFICATION_ARB                   0x8261
#define GLX_LOSE_CONTEXT_ON_RESET_ARB                   0x8252
#endif

#ifndef GLX_ARB_robustness_application_isolation
#define GLX_ARB_robustness_application_isolation
#define GLX_CONTEXT_RESET_ISOLATION_BIT_ARB             0x00000008
#endif
#ifndef GLX_ARB_robustness_share_group_isolation
#define GLX_ARB_robustness_share_group_isolation
#endif

#ifndef GLX_ARB_context_flush_control
#define GLX_ARB_context_flush_control
#define GLX_CONTEXT_RELEASE_BEHAVIOR_ARB            0x2097
#define GLX_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB       0
#define GLX_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB      0x2098
#endif

#ifndef GLX_EXT_create_context_es2_profile
#define GLX_EXT_create_context_es2_profile
#define GLX_CONTEXT_ES2_PROFILE_BIT_EXT    0x00000004
#endif

#ifndef GLX_EXT_create_context_es_profile
#define GLX_EXT_create_context_es_profile
#define GLX_CONTEXT_ES2_PROFILE_BIT_EXT    0x00000004
#endif

// ----------------------------------------------------------------------------
// wxGLContextAttrs: OpenGL rendering context attributes
// ----------------------------------------------------------------------------
// GLX specific values

wxGLContextAttrs& wxGLContextAttrs::CoreProfile()
{
    AddAttribBits(GLX_CONTEXT_PROFILE_MASK_ARB,
                  GLX_CONTEXT_CORE_PROFILE_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::MajorVersion(int val)
{
    if ( val > 0 )
    {
        AddAttribute(GLX_CONTEXT_MAJOR_VERSION_ARB);
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
        AddAttribute(GLX_CONTEXT_MINOR_VERSION_ARB);
        AddAttribute(val);
    }
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::CompatibilityProfile()
{
    AddAttribBits(GLX_CONTEXT_PROFILE_MASK_ARB,
                  GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::ForwardCompatible()
{
    AddAttribBits(GLX_CONTEXT_FLAGS_ARB,
                  GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::ES2()
{
    AddAttribBits(GLX_CONTEXT_PROFILE_MASK_ARB,
                  GLX_CONTEXT_ES2_PROFILE_BIT_EXT);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::DebugCtx()
{
    AddAttribBits(GLX_CONTEXT_FLAGS_ARB,
                  GLX_CONTEXT_DEBUG_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::Robust()
{
    AddAttribBits(GLX_CONTEXT_FLAGS_ARB,
                  GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::NoResetNotify()
{
    AddAttribute(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
    AddAttribute(GLX_NO_RESET_NOTIFICATION_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::LoseOnReset()
{
    AddAttribute(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
    AddAttribute(GLX_LOSE_CONTEXT_ON_RESET_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::ResetIsolation()
{
    AddAttribBits(GLX_CONTEXT_FLAGS_ARB,
                  GLX_CONTEXT_RESET_ISOLATION_BIT_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::ReleaseFlush(int val)
{
    AddAttribute(GLX_CONTEXT_RELEASE_BEHAVIOR_ARB);
    if ( val == 1 )
        AddAttribute(GLX_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB);
    else
        AddAttribute(GLX_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB);
    SetNeedsARB();
    return *this;
}

wxGLContextAttrs& wxGLContextAttrs::PlatformDefaults()
{
    renderTypeRGBA = true;
    x11Direct = true;
    return *this;
}

void wxGLContextAttrs::EndList()
{
    AddAttribute(None);
}

// ----------------------------------------------------------------------------
// wxGLAttributes: Visual/FBconfig attributes
// ----------------------------------------------------------------------------
// GLX specific values

//   Different versions of GLX API use rather different attributes lists, see
//   the following URLs:
//
//   - <= 1.2: http://www.opengl.org/sdk/docs/man/xhtml/glXChooseVisual.xml
//   - >= 1.3: http://www.opengl.org/sdk/docs/man/xhtml/glXChooseFBConfig.xml
//
//   Notice in particular that
//   - GLX_RGBA is boolean attribute in the old version of the API but a
//     value of GLX_RENDER_TYPE in the new one
//   - Boolean attributes such as GLX_DOUBLEBUFFER don't take values in the
//     old version but must be followed by True or False in the new one.

wxGLAttributes& wxGLAttributes::RGBA()
{
    if ( wxGLCanvasX11::GetGLXVersion() >= 13 )
        AddAttribBits(GLX_RENDER_TYPE, GLX_RGBA_BIT);
    else
        AddAttribute(GLX_RGBA);
    return *this;
}

wxGLAttributes& wxGLAttributes::BufferSize(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(GLX_BUFFER_SIZE);
        AddAttribute(val);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::Level(int val)
{
    AddAttribute(GLX_LEVEL);
    AddAttribute(val);
    return *this;
}

wxGLAttributes& wxGLAttributes::DoubleBuffer()
{
    AddAttribute(GLX_DOUBLEBUFFER);
    if ( wxGLCanvasX11::GetGLXVersion() >= 13 )
        AddAttribute(True);
    return *this;
}

wxGLAttributes& wxGLAttributes::Stereo()
{
    AddAttribute(GLX_STEREO);
    if ( wxGLCanvasX11::GetGLXVersion() >= 13 )
        AddAttribute(True);
    return *this;
}

wxGLAttributes& wxGLAttributes::AuxBuffers(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(GLX_AUX_BUFFERS);
        AddAttribute(val);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::MinRGBA(int mRed, int mGreen, int mBlue, int mAlpha)
{
    if ( mRed >= 0)
    {
        AddAttribute(GLX_RED_SIZE);
        AddAttribute(mRed);
    }
    if ( mGreen >= 0)
    {
        AddAttribute(GLX_GREEN_SIZE);
        AddAttribute(mGreen);
    }
    if ( mBlue >= 0)
    {
        AddAttribute(GLX_BLUE_SIZE);
        AddAttribute(mBlue);
    }
    if ( mAlpha >= 0)
    {
        AddAttribute(GLX_ALPHA_SIZE);
        AddAttribute(mAlpha);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::Depth(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(GLX_DEPTH_SIZE);
        AddAttribute(val);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::Stencil(int val)
{
    if ( val >= 0 )
    {
        AddAttribute(GLX_STENCIL_SIZE);
        AddAttribute(val);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::MinAcumRGBA(int mRed, int mGreen, int mBlue, int mAlpha)
{
    if ( mRed >= 0)
    {
        AddAttribute(GLX_ACCUM_RED_SIZE);
        AddAttribute(mRed);
    }
    if ( mGreen >= 0)
    {
        AddAttribute(GLX_ACCUM_GREEN_SIZE);
        AddAttribute(mGreen);
    }
    if ( mBlue >= 0)
    {
        AddAttribute(GLX_ACCUM_BLUE_SIZE);
        AddAttribute(mBlue);
    }
    if ( mAlpha >= 0)
    {
        AddAttribute(GLX_ACCUM_ALPHA_SIZE);
        AddAttribute(mAlpha);
    }
    return *this;
}

wxGLAttributes& wxGLAttributes::SampleBuffers(int val)
{
#ifdef GLX_SAMPLE_BUFFERS_ARB
    if ( val >= 0 && wxGLCanvasX11::IsGLXMultiSampleAvailable() )
    {
        AddAttribute(GLX_SAMPLE_BUFFERS_ARB);
        AddAttribute(val);
    }
#endif
    return *this;
}

wxGLAttributes& wxGLAttributes::Samplers(int val)
{
#ifdef GLX_SAMPLES_ARB
    if ( val >= 0 && wxGLCanvasX11::IsGLXMultiSampleAvailable() )
    {
        AddAttribute(GLX_SAMPLES_ARB);
        AddAttribute(val);
    }
#endif
    return *this;
}

wxGLAttributes& wxGLAttributes::FrameBuffersRGB()
{
    AddAttribute(GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB);
    AddAttribute(True);
    return *this;
}

void wxGLAttributes::EndList()
{
    AddAttribute(None);
}

wxGLAttributes& wxGLAttributes::PlatformDefaults()
{
    // No GLX specific values
    return *this;
}

wxGLAttributes& wxGLAttributes::Defaults()
{
    RGBA().DoubleBuffer();
    if ( wxGLCanvasX11::GetGLXVersion() < 13 )
        Depth(1).MinRGBA(1, 1, 1, 0);
    else
        Depth(16).SampleBuffers(1).Samplers(4);
    return *this;
}


// ============================================================================
// wxGLContext implementation
// ============================================================================

// Need this X error handler for the case context creation fails
static bool g_ctxErrorOccurred = false;
static int CTXErrorHandler( Display* WXUNUSED(dpy), XErrorEvent* WXUNUSED(ev) )
{
    g_ctxErrorOccurred = true;
    return 0;
}

wxIMPLEMENT_CLASS(wxGLContext, wxObject);

wxGLContext::wxGLContext(wxGLCanvas *win,
                         const wxGLContext *other,
                         const wxGLContextAttrs *ctxAttrs)
    : m_glContext(NULL)
{
    const int* contextAttribs = NULL;
    Bool x11Direct = True;
    int renderType = GLX_RGBA_TYPE;
    bool needsARB = false;

    if ( ctxAttrs )
    {
        contextAttribs = ctxAttrs->GetGLAttrs();
        x11Direct = ctxAttrs->x11Direct;
        renderType = ctxAttrs->renderTypeRGBA ? GLX_RGBA_TYPE : GLX_COLOR_INDEX_TYPE;
        needsARB = ctxAttrs->NeedsARB();
    }
    else if ( win->GetGLCTXAttrs().GetGLAttrs() )
    {
        // If OpenGL context parameters were set at wxGLCanvas ctor, get them now
        contextAttribs = win->GetGLCTXAttrs().GetGLAttrs();
        x11Direct = win->GetGLCTXAttrs().x11Direct;
        renderType = win->GetGLCTXAttrs().renderTypeRGBA ? GLX_RGBA_TYPE : GLX_COLOR_INDEX_TYPE;
        needsARB = win->GetGLCTXAttrs().NeedsARB();
    }
    // else use GPU driver defaults and x11Direct renderType ones

    m_isOk = false;

    Display* dpy = wxGetX11Display();
    XVisualInfo *vi = win->GetXVisualInfo();
    wxCHECK_RET( vi, "invalid visual for OpenGL" );

    // We need to create a temporary context to get the
    // glXCreateContextAttribsARB function
    GLXContext tempContext = glXCreateContext(dpy, vi, NULL,
                                              win->GetGLCTXAttrs().x11Direct );
    wxCHECK_RET(tempContext, "glXCreateContext failed" );

    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB
        = (PFNGLXCREATECONTEXTATTRIBSARBPROC)
        glXGetProcAddress((GLubyte *)"glXCreateContextAttribsARB");

    glXDestroyContext( dpy, tempContext );

    // The preferred way is using glXCreateContextAttribsARB, even for old context
    if ( !glXCreateContextAttribsARB && needsARB ) // OpenGL 3 context creation
    {
        wxLogMessage(_("OpenGL 3.0 or later is not supported by the OpenGL driver."));
        return;
    }

    // Install a X error handler, so as to the app doesn't exit (without
    // even a warning) if GL >= 3.0 context creation fails
    g_ctxErrorOccurred = false;
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&CTXErrorHandler);

    if ( glXCreateContextAttribsARB )
    {
        GLXFBConfig *fbc = win->GetGLXFBConfig();
        wxCHECK_RET( fbc, "Invalid GLXFBConfig for OpenGL" );

        m_glContext = glXCreateContextAttribsARB( dpy, fbc[0],
                                other ? other->m_glContext : None,
                                x11Direct, contextAttribs );
    }
    else if ( wxGLCanvas::GetGLXVersion() >= 13 )
    {
        GLXFBConfig *fbc = win->GetGLXFBConfig();
        wxCHECK_RET( fbc, "Invalid GLXFBConfig for OpenGL" );

        m_glContext = glXCreateNewContext( dpy, fbc[0], renderType,
                                           other ? other->m_glContext : None,
                                           x11Direct );
    }
    else // GLX <= 1.2
    {
        m_glContext = glXCreateContext( dpy, vi,
                                        other ? other->m_glContext : None,
                                        x11Direct );
    }

    // Sync to ensure any errors generated are processed.
    XSync( dpy, False );

    if ( g_ctxErrorOccurred || !m_glContext )
        wxLogMessage(_("Couldn't create OpenGL context"));
    else
        m_isOk = true;

    // Restore old error handler
    XSetErrorHandler( oldHandler );
}

wxGLContext::~wxGLContext()
{
    if ( !m_glContext )
        return;

    if ( m_glContext == glXGetCurrentContext() )
        MakeCurrent(None, NULL);

    glXDestroyContext( wxGetX11Display(), m_glContext );
}

bool wxGLContext::SetCurrent(const wxGLCanvas& win) const
{
    if ( !m_glContext )
        return false;

    const Window xid = win.GetXWindow();
    wxCHECK2_MSG( xid, return false, wxT("window must be shown") );

    return MakeCurrent(xid, m_glContext);
}

// wrapper around glXMakeContextCurrent/glXMakeCurrent depending on GLX
// version
/* static */
bool wxGLContext::MakeCurrent(GLXDrawable drawable, GLXContext context)
{
    if (wxGLCanvas::GetGLXVersion() >= 13)
        return glXMakeContextCurrent( wxGetX11Display(), drawable, drawable, context);
    else // GLX <= 1.2 doesn't have glXMakeContextCurrent()
        return glXMakeCurrent( wxGetX11Display(), drawable, context);
}

// ============================================================================
// wxGLCanvasX11 implementation
// ============================================================================

// ----------------------------------------------------------------------------
// initialization methods and dtor
// ----------------------------------------------------------------------------

wxGLCanvasX11::wxGLCanvasX11()
{
    m_fbc = NULL;
    m_vi = NULL;
}

bool wxGLCanvasX11::InitVisual(const wxGLAttributes& dispAttrs)
{
    bool ret = InitXVisualInfo(dispAttrs, &m_fbc, &m_vi);
    if ( !ret )
    {
        wxFAIL_MSG("Failed to get a XVisualInfo for the requested attributes.");
    }
    return ret;
}

wxGLCanvasX11::~wxGLCanvasX11()
{
    if ( m_fbc && m_fbc != ms_glFBCInfo )
        XFree(m_fbc);

    if ( m_vi && m_vi != ms_glVisualInfo )
        XFree(m_vi);
}

// ----------------------------------------------------------------------------
// working with GL attributes
// ----------------------------------------------------------------------------

/* static */
bool wxGLCanvasBase::IsExtensionSupported(const char *extension)
{
    Display * const dpy = wxGetX11Display();

    return IsExtensionInList(glXQueryExtensionsString(dpy, DefaultScreen(dpy)),
                             extension);
}


/* static */
bool wxGLCanvasX11::IsGLXMultiSampleAvailable()
{
    static int s_isMultiSampleAvailable = -1;
    if ( s_isMultiSampleAvailable == -1 )
        s_isMultiSampleAvailable = IsExtensionSupported("GLX_ARB_multisample");

    return s_isMultiSampleAvailable != 0;
}


/* static */
bool wxGLCanvasX11::InitXVisualInfo(const wxGLAttributes& dispAttrs,
                                    GLXFBConfig** pFBC,
                                    XVisualInfo** pXVisual)
{
    // GLX_XX attributes
    const int* attrsListGLX = dispAttrs.GetGLAttrs();
    if ( !attrsListGLX )
    {
        wxFAIL_MSG("wxGLAttributes object is empty.");
        return false;
    }

    Display* dpy = wxGetX11Display();

    if ( GetGLXVersion() >= 13 )
    {
        int returned;
        *pFBC = glXChooseFBConfig(dpy, DefaultScreen(dpy), attrsListGLX, &returned);

        if ( *pFBC )
        {
            // Use the first good match
            *pXVisual = glXGetVisualFromFBConfig(wxGetX11Display(), **pFBC);
            if ( !*pXVisual )
            {
                XFree(*pFBC);
                *pFBC = NULL;
            }
        }
    }
    else // GLX <= 1.2
    {
        *pFBC = NULL;
        *pXVisual = glXChooseVisual(dpy, DefaultScreen(dpy),
                                   wx_const_cast(int*, attrsListGLX) );
    }

    return *pXVisual != NULL;
}

/* static */
bool wxGLCanvasBase::IsDisplaySupported(const wxGLAttributes& dispAttrs)
{
    GLXFBConfig *fbc = NULL;
    XVisualInfo *vi = NULL;

    bool isSupported = wxGLCanvasX11::InitXVisualInfo(dispAttrs, &fbc, &vi);

    if ( fbc )
        XFree(fbc);
    if ( vi )
        XFree(vi);

    return isSupported;
}

/* static */
bool wxGLCanvasBase::IsDisplaySupported(const int *attribList)
{
    wxGLAttributes dispAttrs;
    ParseAttribList(attribList, dispAttrs);

    return IsDisplaySupported(dispAttrs);
}

// ----------------------------------------------------------------------------
// default visual management
// ----------------------------------------------------------------------------

XVisualInfo *wxGLCanvasX11::ms_glVisualInfo = NULL;
GLXFBConfig *wxGLCanvasX11::ms_glFBCInfo = NULL;

/* static */
bool wxGLCanvasX11::InitDefaultVisualInfo(const int *attribList)
{
    FreeDefaultVisualInfo();
    wxGLAttributes dispAttrs;
    ParseAttribList(attribList, dispAttrs);

    return InitXVisualInfo(dispAttrs, &ms_glFBCInfo, &ms_glVisualInfo);
}

/* static */
void wxGLCanvasX11::FreeDefaultVisualInfo()
{
    if ( ms_glFBCInfo )
    {
        XFree(ms_glFBCInfo);
        ms_glFBCInfo = NULL;
    }

    if ( ms_glVisualInfo )
    {
        XFree(ms_glVisualInfo);
        ms_glVisualInfo = NULL;
    }
}

// ----------------------------------------------------------------------------
// other GL methods
// ----------------------------------------------------------------------------

/* static */
int wxGLCanvasX11::GetGLXVersion()
{
    static int s_glxVersion = 0;
    if ( s_glxVersion == 0 )
    {
        // check the GLX version
        int glxMajorVer, glxMinorVer;
        bool ok = glXQueryVersion(wxGetX11Display(), &glxMajorVer, &glxMinorVer);
        wxASSERT_MSG( ok, wxT("GLX version not found") );
        if (!ok)
            s_glxVersion = 10; // 1.0 by default
        else
            s_glxVersion = glxMajorVer*10 + glxMinorVer;
    }

    return s_glxVersion;
}

bool wxGLCanvasX11::SwapBuffers()
{
    const Window xid = GetXWindow();
    wxCHECK2_MSG( xid, return false, wxT("window must be shown") );

    glXSwapBuffers(wxGetX11Display(), xid);
    return true;
}

bool wxGLCanvasX11::IsShownOnScreen() const
{
    return GetXWindow() && wxGLCanvasBase::IsShownOnScreen();
}

#endif // wxUSE_GLCANVAS

