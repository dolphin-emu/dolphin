///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/glcanvas_osx.cpp
// Purpose:     wxGLCanvas, for using OpenGL with wxWidgets under Macintosh
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
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

#include "wx/osx/private.h"

// ----------------------------------------------------------------------------
// wxGLCanvas
// ----------------------------------------------------------------------------

wxGLContext::wxGLContext(wxGLCanvas *win, const wxGLContext *other)
{
    m_glContext = WXGLCreateContext(win->GetWXGLPixelFormat(),
                                    other ? other->m_glContext : NULL);
}

wxGLContext::~wxGLContext()
{
    if ( m_glContext )
    {
        WXGLDestroyContext(m_glContext);
    }
}

// ----------------------------------------------------------------------------
// wxGLCanvas
// ----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGLCanvas, wxWindow);

wxBEGIN_EVENT_TABLE(wxGLCanvas, wxWindow)
#if wxOSX_USE_CARBON
    EVT_SIZE(wxGLCanvas::OnSize)
#endif
wxEND_EVENT_TABLE()

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       wxWindowID id,
                       const int *attribList,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const wxPalette& palette)
{
    Create(parent, id, pos, size, style, name, attribList, palette);
}

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
    if ( Create(parent, id, pos, size, style, name, attribList, palette) )
        m_glContext = new wxGLContext(this, shared ? shared->m_glContext : NULL);
}

#endif // WXWIN_COMPATIBILITY_2_8

/* static */
bool wxGLCanvas::IsAGLMultiSampleAvailable()
{
    static int s_isMultiSampleAvailable = -1;
    if ( s_isMultiSampleAvailable == -1 )
        s_isMultiSampleAvailable = IsExtensionSupported("GL_ARB_multisample");

    return s_isMultiSampleAvailable != 0;
}

/* static */
bool wxGLCanvasBase::IsDisplaySupported(const int *attribList)
{
    WXGLPixelFormat glFormat = WXGLChoosePixelFormat(attribList);

    if ( !glFormat )
       return false;

    WXGLDestroyPixelFormat(glFormat);

    return true;
}

bool wxGLCanvasBase::IsExtensionSupported(const char *extension)
{
    // we need a valid context to query for extensions.
    WXGLPixelFormat fmt = WXGLChoosePixelFormat(NULL);
    WXGLContext ctx = WXGLCreateContext(fmt, NULL);
    if ( !ctx )
        return false;

    WXGLContext ctxOld = WXGLGetCurrentContext();
    WXGLSetCurrentContext(ctx);

    wxString extensions = wxString::FromAscii(glGetString(GL_EXTENSIONS));

    WXGLSetCurrentContext(ctxOld);
    WXGLDestroyPixelFormat(fmt);
    WXGLDestroyContext(ctx);

    return IsExtensionInList(extensions.ToAscii(), extension);
}

// ----------------------------------------------------------------------------
// wxGLApp
// ----------------------------------------------------------------------------

bool wxGLApp::InitGLVisual(const int *attribList)
{
    WXGLPixelFormat fmt = WXGLChoosePixelFormat(attribList);
    if ( !fmt )
        return false;

    WXGLDestroyPixelFormat(fmt);
    return true;
}

#endif // wxUSE_GLCANVAS
