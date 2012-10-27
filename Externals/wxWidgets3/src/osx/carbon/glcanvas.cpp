///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/glcanvas.cpp
// Purpose:     wxGLCanvas, for using OpenGL with wxWidgets under Macintosh
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: glcanvas.cpp 67412 2011-04-07 12:55:36Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#include "wx/glcanvas.h"

#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/log.h"
    #include "wx/settings.h"
#endif

#include "wx/osx/uma.h"

#include "wx/osx/private.h"
#include <AGL/agl.h>

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

static void wxLogAGLError(const char *func)
{
    const int err = aglGetError();

    wxLogError(_("OpenGL function \"%s\" failed: %s (error %d)"),
               func, aglErrorString(err), err);
}

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// low level implementation routines
// ----------------------------------------------------------------------------

WXGLContext WXGLCreateContext( WXGLPixelFormat pixelFormat, WXGLContext shareContext )
{
    WXGLContext context = aglCreateContext(pixelFormat, shareContext);
    if ( !context )
        wxLogAGLError("aglCreateContext");
    return context ;
}

void WXGLDestroyContext( WXGLContext context )
{
    if ( context )
    {
        if ( !aglDestroyContext(context) )
        {
            wxLogAGLError("aglDestroyContext");
        }
    }
}

WXGLContext WXGLGetCurrentContext()
{
    return aglGetCurrentContext();
}

bool WXGLSetCurrentContext(WXGLContext context)
{
    if ( !aglSetCurrentContext(context) )
    {
        wxLogAGLError("aglSetCurrentContext");
        return false;
    }

    return true;
}

void WXGLDestroyPixelFormat( WXGLPixelFormat pixelFormat )
{
    if ( pixelFormat )
    {
        aglDestroyPixelFormat(pixelFormat);
    }
}

WXGLPixelFormat WXGLChoosePixelFormat(const int *attribList)
{
    GLint data[512];
    const GLint defaultAttribs[] =
    {
        AGL_RGBA,
        AGL_DOUBLEBUFFER,
        AGL_MINIMUM_POLICY, // never choose less than requested
        AGL_DEPTH_SIZE, 1,  // use largest available depth buffer
        AGL_RED_SIZE, 1,
        AGL_GREEN_SIZE, 1,
        AGL_BLUE_SIZE, 1,
        AGL_ALPHA_SIZE, 0,
        AGL_NONE
    };

    const GLint *attribs;
    if ( !attribList )
    {
        attribs = defaultAttribs;
    }
    else
    {
        unsigned p = 0;
        data[p++] = AGL_MINIMUM_POLICY; // make _SIZE tags behave more like GLX

        for ( unsigned arg = 0; attribList[arg] !=0 && p < WXSIZEOF(data); )
        {
            switch ( attribList[arg++] )
            {
                case WX_GL_RGBA:
                    data[p++] = AGL_RGBA;
                    break;

                case WX_GL_BUFFER_SIZE:
                    data[p++] = AGL_BUFFER_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_LEVEL:
                    data[p++]=AGL_LEVEL;
                    data[p++]=attribList[arg++];
                    break;

                case WX_GL_DOUBLEBUFFER:
                    data[p++] = AGL_DOUBLEBUFFER;
                    break;

                case WX_GL_STEREO:
                    data[p++] = AGL_STEREO;
                    break;

                case WX_GL_AUX_BUFFERS:
                    data[p++] = AGL_AUX_BUFFERS;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_RED:
                    data[p++] = AGL_RED_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_GREEN:
                    data[p++] = AGL_GREEN_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_BLUE:
                    data[p++] = AGL_BLUE_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_ALPHA:
                    data[p++] = AGL_ALPHA_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_DEPTH_SIZE:
                    data[p++] = AGL_DEPTH_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_STENCIL_SIZE:
                    data[p++] = AGL_STENCIL_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_ACCUM_RED:
                    data[p++] = AGL_ACCUM_RED_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_ACCUM_GREEN:
                    data[p++] = AGL_ACCUM_GREEN_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_ACCUM_BLUE:
                    data[p++] = AGL_ACCUM_BLUE_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_ACCUM_ALPHA:
                    data[p++] = AGL_ACCUM_ALPHA_SIZE;
                    data[p++] = attribList[arg++];
                    break;

                case WX_GL_SAMPLE_BUFFERS:
                    if ( !wxGLCanvas::IsAGLMultiSampleAvailable() )
                    {
                        if ( !attribList[arg++] )
                            break;

                        return NULL;
                    }

                    data[p++] = AGL_SAMPLE_BUFFERS_ARB;
                    if ( (data[p++] = attribList[arg++]) == true )
                    {
                        // don't use software fallback
                        data[p++] = AGL_NO_RECOVERY;
                    }
                    break;

                case WX_GL_SAMPLES:
                    if ( !wxGLCanvas::IsAGLMultiSampleAvailable() )
                    {
                        if ( !attribList[arg++] )
                            break;

                        return NULL;
                    }

                    data[p++] = AGL_SAMPLES_ARB;
                    data[p++] = attribList[arg++];
                    break;
            }
        }

        data[p] = AGL_NONE;

        attribs = data;
    }

    return aglChoosePixelFormat(NULL, 0, attribs);
}

// ----------------------------------------------------------------------------
// wxGLContext
// ----------------------------------------------------------------------------

bool wxGLContext::SetCurrent(const wxGLCanvas& win) const
{
    if ( !m_glContext )
        return false;

    GLint bufnummer = win.GetAglBufferName();
    aglSetInteger(m_glContext, AGL_BUFFER_NAME, &bufnummer);
    //win.SetLastContext(m_glContext);

    const_cast<wxGLCanvas&>(win).SetViewport();

    
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
    if ( UMAGetSystemVersion() >= 0x1050 )
    {
        aglSetWindowRef(m_glContext, win.MacGetTopLevelWindowRef());
    }
    else
#endif
    {
        AGLDrawable drawable = (AGLDrawable)GetWindowPort(
                                                      MAC_WXHWND(win.MacGetTopLevelWindowRef()));
    
        if ( !aglSetDrawable(m_glContext, drawable) )
        {
            wxLogAGLError("aglSetDrawable");
            return false;
        }
    }

    return WXGLSetCurrentContext(m_glContext);
}

// ----------------------------------------------------------------------------
// wxGLCanvas
// ----------------------------------------------------------------------------

/*

sharing contexts under AGL is not straightforward, to quote from

http://lists.apple.com/archives/mac-opengl/2003/Jan/msg00402.html :

In Carbon OpenGL (AGL) you would use call aglSetInteger to setup a
buffer name and attached each context to that same name. From AGL
you can do:

GLint id = 1;

ctx1 = aglCreateContext...
aglSetInteger(ctx1, AGL_BUFFER_NAME, &id); // create name
aglAttachDrawable (ctx1,...); // create surface with associated with
name (first time)
ctx2 = aglCreateContext...
aglSetInteger(ctx2, AGL_BUFFER_NAME, &id); // uses previously created name
aglAttachDrawable (ctx2, ...); // uses existing surface with existing name

AGL Docs say:
AGL_BUFFER_NAME
params contains one value: a non-negative integer name of the surface to be
associated to be with the current context. If this value is non-zero, and a
surface of this name is not associated to this drawable, a new surface with
this name is created and associated with the context when
aglSetDrawable is called subsequently. If this is a previously allocated
buffer name within the namespace of the current window (e.g., drawable),
that previously allocated surface is associated with the context (e.g., no
new surface is created) and the subsequent call to aglSetDrawable will
attach that surface. This allows multiple contexts to be attached to a single
surface. Using the default buffer name zero, returns to one surface per
context behaviour.
*/

/*
so what I'm doing is to have a dummy aglContext attached to a wxGLCanvas,
assign it a buffer number
*/


bool wxGLCanvas::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name,
                        const int *attribList,
                        const wxPalette& WXUNUSED(palette))
{
    m_needsUpdate = false;
    m_macCanvasIsShown = false;

    m_glFormat = WXGLChoosePixelFormat(attribList);
    if ( !m_glFormat )
        return false;

    if ( !wxWindow::Create(parent, id, pos, size, style, name) )
        return false;

    m_dummyContext = WXGLCreateContext(m_glFormat, NULL);

    static GLint gCurrentBufferName = 1;
    m_bufferName = gCurrentBufferName++;
    aglSetInteger (m_dummyContext, AGL_BUFFER_NAME, &m_bufferName);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
    if ( UMAGetSystemVersion() >= 0x1050 )
    {
        aglSetWindowRef(m_dummyContext, MacGetTopLevelWindowRef());
    }
    else
#endif
    {
        AGLDrawable drawable = (AGLDrawable)GetWindowPort(MAC_WXHWND(MacGetTopLevelWindowRef()));
        aglSetDrawable(m_dummyContext, drawable);
    }

    m_macCanvasIsShown = true;

    return true;
}

wxGLCanvas::~wxGLCanvas()
{
    if ( m_glFormat )
        WXGLDestroyPixelFormat(m_glFormat);

    if ( m_dummyContext )
        WXGLDestroyContext(m_dummyContext);
}

bool wxGLCanvas::SwapBuffers()
{
    WXGLContext context = WXGLGetCurrentContext();
    wxCHECK_MSG(context, false, wxT("should have current context"));

    aglSwapBuffers(context);
    return true;
}

void wxGLCanvas::SetViewport()
{
    if ( !m_needsUpdate )
        return;

    m_needsUpdate = false;

//    AGLContext context = aglGetCurrentContext();
//    if ( !context )
//        return;

    // viewport is initially set to entire port, adjust it to just this window
    int x = 0,
        y = 0;
    MacClientToRootWindow(&x , &y);

    int width, height;
    GetClientSize(&width, &height);

    Rect bounds;
    GetWindowPortBounds(MAC_WXHWND(MacGetTopLevelWindowRef()) , &bounds);

    GLint parms[4];
    parms[0] = x;
    parms[1] = bounds.bottom - bounds.top - ( y + height );
    parms[2] = width;
    parms[3] = height;

    // move the buffer rect out of sight if we're hidden
    if ( !m_macCanvasIsShown )
        parms[0] += 20000;

    if ( !aglSetInteger(m_dummyContext, AGL_BUFFER_RECT, parms) )
        wxLogAGLError("aglSetInteger(AGL_BUFFER_RECT)");

    if ( !aglEnable(m_dummyContext, AGL_BUFFER_RECT) )
        wxLogAGLError("aglEnable(AGL_BUFFER_RECT)");

    if ( !aglUpdateContext(m_dummyContext) )
        wxLogAGLError("aglUpdateContext");
}

void wxGLCanvas::OnSize(wxSizeEvent& event)
{
    MacUpdateView();
    event.Skip();
}

void wxGLCanvas::MacUpdateView()
{
    m_needsUpdate = true;
    Refresh(false);
}

void wxGLCanvas::MacSuperChangedPosition()
{
    MacUpdateView();
    SetViewport();
    wxWindow::MacSuperChangedPosition();
}

void wxGLCanvas::MacTopLevelWindowChangedPosition()
{
    MacUpdateView();
    wxWindow::MacTopLevelWindowChangedPosition();
}

void wxGLCanvas::MacVisibilityChanged()
{
    if ( IsShownOnScreen() != m_macCanvasIsShown )
    {
        m_macCanvasIsShown = !m_macCanvasIsShown;
        MacUpdateView();
    }

    wxWindowMac::MacVisibilityChanged();
}

#endif // wxUSE_GLCANVAS
