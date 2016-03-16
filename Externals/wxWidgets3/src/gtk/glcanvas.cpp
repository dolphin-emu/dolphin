/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/glcanvas.cpp
// Purpose:     wxGLCanvas, for using OpenGL/Mesa with wxWidgets and GTK
// Author:      Robert Roebling
// Modified by:
// Created:     17/08/98
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_GLCANVAS

#include "wx/glcanvas.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "wx/gtk/private/gtk2-compat.h"

#if WXWIN_COMPATIBILITY_2_8

//-----------------------------------------------------------------------------
// "realize" from m_wxwindow: used to create m_glContext implicitly
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_glwindow_realized_callback( GtkWidget *WXUNUSED(widget), wxGLCanvas *win )
{
    win->GTKInitImplicitContext();
}
}

#endif // WXWIN_COMPATIBILITY_2_8

//-----------------------------------------------------------------------------
// "map" from m_wxwindow
//-----------------------------------------------------------------------------

#ifndef __WXGTK3__
extern "C" {
static void
gtk_glwindow_map_callback( GtkWidget * WXUNUSED(widget), wxGLCanvas *win )
{
    wxPaintEvent event( win->GetId() );
    event.SetEventObject( win );
    win->HandleWindowEvent( event );

    win->m_exposed = false;
    win->GetUpdateRegion().Clear();
}
}
#endif

//-----------------------------------------------------------------------------
// "expose_event" of m_wxwindow
//-----------------------------------------------------------------------------

extern "C" {
#ifdef __WXGTK3__
static gboolean draw(GtkWidget*, cairo_t* cr, wxGLCanvas* win)
{
    win->m_exposed = true;
    if (win->m_cairoPaintContext == NULL)
    {
        win->m_cairoPaintContext = cr;
        cairo_reference(cr);
    }
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    win->GetUpdateRegion().Union(int(x1), int(y1), int(x2 - x1), int(y2 - y1));
    return false;
}
#else
static gboolean
gtk_glwindow_expose_callback( GtkWidget *WXUNUSED(widget), GdkEventExpose *gdk_event, wxGLCanvas *win )
{
    win->m_exposed = true;

    win->GetUpdateRegion().Union( gdk_event->area.x,
                                  gdk_event->area.y,
                                  gdk_event->area.width,
                                  gdk_event->area.height );
    return false;
}
#endif
}

//-----------------------------------------------------------------------------
// "size_allocate" of m_wxwindow
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_glcanvas_size_callback(GtkWidget *WXUNUSED(widget),
                           GtkAllocation * WXUNUSED(alloc),
                           wxGLCanvas *win)
{
    wxSizeEvent event( wxSize(win->m_width,win->m_height), win->GetId() );
    event.SetEventObject( win );
    win->HandleWindowEvent( event );
}
}

//-----------------------------------------------------------------------------
// emission hook for "parent-set"
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
parent_set_hook(GSignalInvocationHint*, guint, const GValue* param_values, void* data)
{
    wxGLCanvas* win = (wxGLCanvas*)data;
    if (g_value_peek_pointer(&param_values[0]) == win->m_wxwindow)
    {
        const XVisualInfo* xvi = win->GetXVisualInfo();
        GdkVisual* visual = gtk_widget_get_visual(win->m_wxwindow);
        if (GDK_VISUAL_XVISUAL(visual)->visualid != xvi->visualid)
        {
            GdkScreen* screen = gtk_widget_get_screen(win->m_wxwindow);
            visual = gdk_x11_screen_lookup_visual(screen, xvi->visualid);
#ifdef __WXGTK3__
            gtk_widget_set_visual(win->m_wxwindow, visual);
#else
            GdkColormap* colormap = gdk_colormap_new(visual, false);
            gtk_widget_set_colormap(win->m_wxwindow, colormap);
            g_object_unref(colormap);
#endif
        }
        // remove hook
        return false;
    }
    return true;
}
}

//---------------------------------------------------------------------------
// wxGlCanvas
//---------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGLCanvas, wxWindow);

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       const wxGLAttributes& dispAttrs,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const wxPalette& palette)
#if WXWIN_COMPATIBILITY_2_8
    : m_createImplicitContext(false)
#endif
{
    Create(parent, dispAttrs, id, pos, size, style, name, palette);
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       wxWindowID id,
                       const int *attribList,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const wxPalette& palette)
#if WXWIN_COMPATIBILITY_2_8
    : m_createImplicitContext(false)
#endif
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
    : m_createImplicitContext(true)
{
    m_sharedContext = NULL;
    m_sharedContextOf = NULL;

    Create(parent, id, pos, size, style, name, attribList, palette);
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
    : m_createImplicitContext(true)
{
    m_sharedContext = const_cast<wxGLContext *>(shared);

    Create(parent, id, pos, size, style, name, attribList, palette);
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       const wxGLCanvas *shared,
                       wxWindowID id,
                       const wxPoint& pos, const wxSize& size,
                       long style, const wxString& name,
                       const int *attribList,
                       const wxPalette& palette )
    : m_createImplicitContext(true)
{
    m_sharedContext = NULL;
    m_sharedContextOf = const_cast<wxGLCanvas *>(shared);

    Create(parent, id, pos, size, style, name, attribList, palette);
}

#endif // WXWIN_COMPATIBILITY_2_8

bool wxGLCanvas::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name,
                        const int *attribList,
                        const wxPalette& palette)
{
    // Separate 'GLXFBConfig/XVisual' attributes.
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
#if wxUSE_PALETTE
    wxASSERT_MSG( !palette.IsOk(), wxT("palettes not supported") );
#endif // wxUSE_PALETTE
    wxUnusedVar(palette); // Unused when wxDEBUG_LEVEL==0

    m_exposed = false;
    m_noExpose = true;
    m_nativeSizeEvent = true;
#ifdef __WXGTK3__
    m_cairoPaintContext = NULL;
    m_backgroundStyle = wxBG_STYLE_PAINT;
#endif

    if ( !InitVisual(dispAttrs) )
        return false;

    // watch for the "parent-set" signal on m_wxwindow so we can set colormap
    // before m_wxwindow is realized (which will occur before
    // wxWindow::Create() returns if parent is already visible)
    unsigned sig_id = g_signal_lookup("parent-set", GTK_TYPE_WIDGET);
    g_signal_add_emission_hook(sig_id, 0, parent_set_hook, this, NULL);

    wxWindow::Create( parent, id, pos, size, style, name );

    gtk_widget_set_double_buffered(m_wxwindow, false);

#if WXWIN_COMPATIBILITY_2_8
    g_signal_connect(m_wxwindow, "realize",       G_CALLBACK(gtk_glwindow_realized_callback), this);
#endif // WXWIN_COMPATIBILITY_2_8
#ifdef __WXGTK3__
    g_signal_connect(m_wxwindow, "draw", G_CALLBACK(draw), this);
#else
    g_signal_connect(m_wxwindow, "map",           G_CALLBACK(gtk_glwindow_map_callback),      this);
    g_signal_connect(m_wxwindow, "expose_event",  G_CALLBACK(gtk_glwindow_expose_callback),   this);
#endif
    g_signal_connect(m_widget,   "size_allocate", G_CALLBACK(gtk_glcanvas_size_callback),     this);

#if WXWIN_COMPATIBILITY_2_8
    // if our parent window is already visible, we had been realized before we
    // connected to the "realize" signal and hence our m_glContext hasn't been
    // initialized yet and we have to do it now
    if (gtk_widget_get_realized(m_wxwindow))
        gtk_glwindow_realized_callback( m_wxwindow, this );
#endif // WXWIN_COMPATIBILITY_2_8

#ifndef __WXGTK3__
    if (gtk_widget_get_mapped(m_wxwindow))
        gtk_glwindow_map_callback( m_wxwindow, this );
#endif

    return true;
}

bool wxGLCanvas::SetBackgroundStyle(wxBackgroundStyle /* style */)
{
    return false;
}

Window wxGLCanvas::GetXWindow() const
{
    GdkWindow* window = GTKGetDrawingWindow();
    return window ? GDK_WINDOW_XID(window) : 0;
}

void wxGLCanvas::OnInternalIdle()
{
    if (m_exposed)
    {
#ifdef __WXGTK3__
        GTKSendPaintEvents(m_cairoPaintContext);
        cairo_destroy(m_cairoPaintContext);
        m_cairoPaintContext = NULL;
#else
        wxPaintEvent event( GetId() );
        event.SetEventObject( this );
        HandleWindowEvent( event );
#endif

        m_exposed = false;
        GetUpdateRegion().Clear();
    }

    wxWindow::OnInternalIdle();
}

#if WXWIN_COMPATIBILITY_2_8

void wxGLCanvas::GTKInitImplicitContext()
{
    if ( !m_glContext && m_createImplicitContext )
    {
        wxGLContext *share = m_sharedContext;
        if ( !share && m_sharedContextOf )
            share = m_sharedContextOf->m_glContext;

        m_glContext = new wxGLContext(this, share);
    }
}

#endif // WXWIN_COMPATIBILITY_2_8

#endif // wxUSE_GLCANVAS
