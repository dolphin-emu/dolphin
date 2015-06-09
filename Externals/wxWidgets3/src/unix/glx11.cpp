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

// ============================================================================
// wxGLContext implementation
// ============================================================================

IMPLEMENT_CLASS(wxGLContext, wxObject)

wxGLContext::wxGLContext(wxGLCanvas *gc, const wxGLContext *other)
{
    if ( wxGLCanvas::GetGLXVersion() >= 13 )
    {
        GLXFBConfig *fbc = gc->GetGLXFBConfig();
        wxCHECK_RET( fbc, wxT("invalid GLXFBConfig for OpenGL") );

        m_glContext = glXCreateNewContext( wxGetX11Display(), fbc[0], GLX_RGBA_TYPE,
                                           other ? other->m_glContext : None,
                                           GL_TRUE );
    }
    else // GLX <= 1.2
    {
        XVisualInfo *vi = gc->GetXVisualInfo();
        wxCHECK_RET( vi, wxT("invalid visual for OpenGL") );

        m_glContext = glXCreateContext( wxGetX11Display(), vi,
                                        other ? other->m_glContext : None,
                                        GL_TRUE );
    }

    wxASSERT_MSG( m_glContext, wxT("Couldn't create OpenGL context") );
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

bool wxGLCanvasX11::InitVisual(const int *attribList)
{
    return InitXVisualInfo(attribList, &m_fbc, &m_vi);
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

bool
wxGLCanvasX11::ConvertWXAttrsToGL(const int *wxattrs, int *glattrs, size_t n)
{
    wxCHECK_MSG( n >= 16, false, wxT("GL attributes buffer too small") );

    /*
       Different versions of GLX API use rather different attributes lists, see
       the following URLs:

        - <= 1.2: http://www.opengl.org/sdk/docs/man/xhtml/glXChooseVisual.xml
        - >= 1.3: http://www.opengl.org/sdk/docs/man/xhtml/glXChooseFBConfig.xml

       Notice in particular that
        - GLX_RGBA is boolean attribute in the old version of the API but a
          value of GLX_RENDER_TYPE in the new one
        - Boolean attributes such as GLX_DOUBLEBUFFER don't take values in the
          old version but must be followed by True or False in the new one.
     */

    if ( !wxattrs )
    {
        size_t i = 0;

        // use double-buffered true colour by default
        glattrs[i++] = GLX_DOUBLEBUFFER;

        if ( GetGLXVersion() < 13 )
        {
            // default settings if attriblist = 0
            glattrs[i++] = GLX_RGBA;
            glattrs[i++] = GLX_DEPTH_SIZE;   glattrs[i++] = 1;
            glattrs[i++] = GLX_RED_SIZE;     glattrs[i++] = 1;
            glattrs[i++] = GLX_GREEN_SIZE;   glattrs[i++] = 1;
            glattrs[i++] = GLX_BLUE_SIZE;    glattrs[i++] = 1;
            glattrs[i++] = GLX_ALPHA_SIZE;   glattrs[i++] = 0;
        }
        else // recent GLX can choose the defaults on its own just fine
        {
            // we just need to have a value after GLX_DOUBLEBUFFER
            glattrs[i++] = True;
        }

        glattrs[i] = None;

        wxASSERT_MSG( i < n, wxT("GL attributes buffer too small") );
    }
    else // have non-default attributes
    {
        size_t p = 0;
        for ( int arg = 0; wxattrs[arg] != 0; )
        {
            // check if we have any space left, knowing that we may insert 2
            // more elements during this loop iteration and we always need to
            // terminate the list with None (hence -3)
            if ( p > n - 3 )
                return false;

            // indicates whether we have a boolean attribute
            bool isBoolAttr = false;

            switch ( wxattrs[arg++] )
            {
                case WX_GL_BUFFER_SIZE:
                    glattrs[p++] = GLX_BUFFER_SIZE;
                    break;

                case WX_GL_LEVEL:
                    glattrs[p++] = GLX_LEVEL;
                    break;

                case WX_GL_RGBA:
                    if ( GetGLXVersion() >= 13 )
                    {
                        // this is the default GLX_RENDER_TYPE anyhow
                        continue;
                    }

                    glattrs[p++] = GLX_RGBA;
                    isBoolAttr = true;
                    break;

                case WX_GL_DOUBLEBUFFER:
                    glattrs[p++] = GLX_DOUBLEBUFFER;
                    isBoolAttr = true;
                    break;

                case WX_GL_STEREO:
                    glattrs[p++] = GLX_STEREO;
                    isBoolAttr = true;
                    break;

                case WX_GL_AUX_BUFFERS:
                    glattrs[p++] = GLX_AUX_BUFFERS;
                    break;

                case WX_GL_MIN_RED:
                    glattrs[p++] = GLX_RED_SIZE;
                    break;

                case WX_GL_MIN_GREEN:
                    glattrs[p++] = GLX_GREEN_SIZE;
                    break;

                case WX_GL_MIN_BLUE:
                    glattrs[p++] = GLX_BLUE_SIZE;
                    break;

                case WX_GL_MIN_ALPHA:
                    glattrs[p++] = GLX_ALPHA_SIZE;
                    break;

                case WX_GL_DEPTH_SIZE:
                    glattrs[p++] = GLX_DEPTH_SIZE;
                    break;

                case WX_GL_STENCIL_SIZE:
                    glattrs[p++] = GLX_STENCIL_SIZE;
                    break;

                case WX_GL_MIN_ACCUM_RED:
                    glattrs[p++] = GLX_ACCUM_RED_SIZE;
                    break;

                case WX_GL_MIN_ACCUM_GREEN:
                    glattrs[p++] = GLX_ACCUM_GREEN_SIZE;
                    break;

                case WX_GL_MIN_ACCUM_BLUE:
                    glattrs[p++] = GLX_ACCUM_BLUE_SIZE;
                    break;

                case WX_GL_MIN_ACCUM_ALPHA:
                    glattrs[p++] = GLX_ACCUM_ALPHA_SIZE;
                    break;

                case WX_GL_SAMPLE_BUFFERS:
#ifdef GLX_SAMPLE_BUFFERS_ARB
                    if ( IsGLXMultiSampleAvailable() )
                    {
                        glattrs[p++] = GLX_SAMPLE_BUFFERS_ARB;
                        break;
                    }
#endif // GLX_SAMPLE_BUFFERS_ARB
                    // if it was specified just to disable it, no problem
                    if ( !wxattrs[arg++] )
                        continue;

                    // otherwise indicate that it's not supported
                    return false;

                case WX_GL_SAMPLES:
#ifdef GLX_SAMPLES_ARB
                    if ( IsGLXMultiSampleAvailable() )
                    {
                        glattrs[p++] = GLX_SAMPLES_ARB;
                        break;
                    }
#endif // GLX_SAMPLES_ARB

                    if ( !wxattrs[arg++] )
                        continue;

                    return false;

                default:
                    wxLogDebug(wxT("Unsupported OpenGL attribute %d"),
                               wxattrs[arg - 1]);
                    continue;
            }

            if ( isBoolAttr )
            {
                // as explained above, for pre 1.3 API the attribute just needs
                // to be present so we only add its value when using the new API
                if ( GetGLXVersion() >= 13 )
                    glattrs[p++] = True;
            }
            else // attribute with real (non-boolean) value
            {
                // copy attribute value as is
                glattrs[p++] = wxattrs[arg++];
            }
        }

        glattrs[p] = None;
    }

    return true;
}

/* static */
bool
wxGLCanvasX11::InitXVisualInfo(const int *attribList,
                               GLXFBConfig **pFBC,
                               XVisualInfo **pXVisual)
{
    int data[512];
    if ( !ConvertWXAttrsToGL(attribList, data, WXSIZEOF(data)) )
        return false;

    Display * const dpy = wxGetX11Display();

    if ( GetGLXVersion() >= 13 )
    {
        int returned;
        *pFBC = glXChooseFBConfig(dpy, DefaultScreen(dpy), data, &returned);

        if ( *pFBC )
        {
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
        *pXVisual = glXChooseVisual(dpy, DefaultScreen(dpy), data);
    }

    return *pXVisual != NULL;
}

/* static */
bool
wxGLCanvasBase::IsDisplaySupported(const int *attribList)
{
    GLXFBConfig *fbc = NULL;
    XVisualInfo *vi = NULL;

    const bool
        isSupported = wxGLCanvasX11::InitXVisualInfo(attribList, &fbc, &vi);

    if ( fbc )
        XFree(fbc);
    if ( vi )
        XFree(vi);

    return isSupported;
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

    return InitXVisualInfo(attribList, &ms_glFBCInfo, &ms_glVisualInfo);
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

