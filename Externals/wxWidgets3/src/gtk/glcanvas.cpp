/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/glcanvas.cpp
// Purpose:     wxGLCanvas, for using OpenGL/Mesa with wxWidgets and GTK
// Author:      Robert Roebling
// Modified by:
// Created:     17/08/98
// RCS-ID:      $Id: glcanvas.cpp 66657 2011-01-08 18:05:33Z PC $
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_GLCANVAS

#include "wx/glcanvas.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

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

//-----------------------------------------------------------------------------
// "expose_event" of m_wxwindow
//-----------------------------------------------------------------------------

extern "C" {
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
    if (!win->m_hasVMT)
        return;

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
            GdkColormap* colormap = gdk_colormap_new(visual, false);
            gtk_widget_set_colormap(win->m_wxwindow, colormap);
            g_object_unref(colormap);
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

IMPLEMENT_CLASS(wxGLCanvas, wxWindow)

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
#if wxUSE_PALETTE
    wxASSERT_MSG( !palette.IsOk(), wxT("palettes not supported") );
#endif // wxUSE_PALETTE
    wxUnusedVar(palette); // Unused when wxDEBUG_LEVEL==0

    m_exposed = false;
    m_noExpose = true;
    m_nativeSizeEvent = true;

    if ( !InitVisual(attribList) )
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
    g_signal_connect(m_wxwindow, "map",           G_CALLBACK(gtk_glwindow_map_callback),      this);
    g_signal_connect(m_wxwindow, "expose_event",  G_CALLBACK(gtk_glwindow_expose_callback),   this);
    g_signal_connect(m_widget,   "size_allocate", G_CALLBACK(gtk_glcanvas_size_callback),     this);

#if WXWIN_COMPATIBILITY_2_8
    // if our parent window is already visible, we had been realized before we
    // connected to the "realize" signal and hence our m_glContext hasn't been
    // initialized yet and we have to do it now
    if (GTK_WIDGET_REALIZED(m_wxwindow))
        gtk_glwindow_realized_callback( m_wxwindow, this );
#endif // WXWIN_COMPATIBILITY_2_8

    if (GTK_WIDGET_MAPPED(m_wxwindow))
        gtk_glwindow_map_callback( m_wxwindow, this );

    return true;
}

Window wxGLCanvas::GetXWindow() const
{
    GdkWindow* window = GTKGetDrawingWindow();
    return window ? GDK_WINDOW_XWINDOW(window) : 0;
}

void wxGLCanvas::OnInternalIdle()
{
    if (m_exposed)
    {
        wxPaintEvent event( GetId() );
        event.SetEventObject( this );
        HandleWindowEvent( event );

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
