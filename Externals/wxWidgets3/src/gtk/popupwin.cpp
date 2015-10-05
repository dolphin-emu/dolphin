/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/popupwin.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_POPUPWIN

#include "wx/popupwin.h"

#ifndef WX_PRECOMP
#endif // WX_PRECOMP

#include <gtk/gtk.h>

#include "wx/gtk/private/win_gtk.h"

//-----------------------------------------------------------------------------
// "button_press"
//-----------------------------------------------------------------------------

extern "C" {
static gint gtk_popup_button_press (GtkWidget *widget, GdkEvent *gdk_event, wxPopupWindow* win )
{
    GtkWidget *child = gtk_get_event_widget (gdk_event);

    /* Ignore events sent out before we connected to the signal */
    if (win->m_time >= ((GdkEventButton*)gdk_event)->time)
        return FALSE;

    /*  We don't ask for button press events on the grab widget, so
     *  if an event is reported directly to the grab widget, it must
     *  be on a window outside the application (and thus we remove
     *  the popup window). Otherwise, we check if the widget is a child
     *  of the grab widget, and only remove the popup window if it
     *  is not. */
    if (child != widget)
    {
        while (child)
        {
            if (child == widget)
                return FALSE;
            child = gtk_widget_get_parent(child);
        }
    }

    wxFocusEvent event( wxEVT_KILL_FOCUS, win->GetId() );
    event.SetEventObject( win );

    (void)win->HandleWindowEvent( event );

    return TRUE;
}
}

//-----------------------------------------------------------------------------
// "delete_event"
//-----------------------------------------------------------------------------

extern "C" {
bool gtk_dialog_delete_callback( GtkWidget *WXUNUSED(widget), GdkEvent *WXUNUSED(event), wxPopupWindow *win )
{
    if (win->IsEnabled())
        win->Close();

    return TRUE;
}
}

//-----------------------------------------------------------------------------
// wxPopupWindow
//-----------------------------------------------------------------------------

#ifdef __WXUNIVERSAL__
wxBEGIN_EVENT_TABLE(wxPopupWindow,wxPopupWindowBase)
    EVT_SIZE(wxPopupWindow::OnSize)
wxEND_EVENT_TABLE()
#endif

wxPopupWindow::~wxPopupWindow()
{
}

bool wxPopupWindow::Create( wxWindow *parent, int style )
{
    if (!PreCreation( parent, wxDefaultPosition, wxDefaultSize ) ||
        !CreateBase( parent, -1, wxDefaultPosition, wxDefaultSize, style, wxDefaultValidator, wxT("popup") ))
    {
        wxFAIL_MSG( wxT("wxPopupWindow creation failed") );
        return false;
    }

    // Unlike windows, top level windows are created hidden by default.
    m_isShown = false;

    // All dialogs should really have this style
    m_windowStyle |= wxTAB_TRAVERSAL;

    m_widget = gtk_window_new( GTK_WINDOW_POPUP );
    g_object_ref( m_widget );

    gtk_widget_set_name( m_widget, "wxPopupWindow" );
    // wxPopupWindow is used for different windows as well
    // gtk_window_set_type_hint( GTK_WINDOW(m_widget), GDK_WINDOW_TYPE_HINT_COMBO );

    // Popup windows can be created without parent, so handle this correctly.
    if (parent)
    {
        GtkWidget *toplevel = gtk_widget_get_toplevel( parent->m_widget );
        if (GTK_IS_WINDOW (toplevel))
            gtk_window_set_transient_for (GTK_WINDOW (m_widget), GTK_WINDOW (toplevel));
    }

    gtk_window_set_resizable (GTK_WINDOW (m_widget), FALSE);

    g_signal_connect (m_widget, "delete_event",
                      G_CALLBACK (gtk_dialog_delete_callback), this);

    m_wxwindow = wxPizza::New();
    gtk_widget_show( m_wxwindow );

    gtk_container_add( GTK_CONTAINER(m_widget), m_wxwindow );

    if (m_parent) m_parent->AddChild( this );

    PostCreation();

    m_time = gtk_get_current_event_time();

    g_signal_connect (m_widget, "button_press_event",
                      G_CALLBACK (gtk_popup_button_press), this);

    return true;
}

void wxPopupWindow::DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y), int WXUNUSED(width), int WXUNUSED(height) )
{
    wxFAIL_MSG( wxT("DoMoveWindow called for wxPopupWindow") );
}

void wxPopupWindow::DoSetSize( int x, int y, int width, int height, int sizeFlags )
{
    wxASSERT_MSG( (m_widget != NULL), wxT("invalid dialog") );
    wxASSERT_MSG( (m_wxwindow != NULL), wxT("invalid dialog") );

    int old_x = m_x;
    int old_y = m_y;

    int old_width = m_width;
    int old_height = m_height;

    if (x != -1 || (sizeFlags & wxSIZE_ALLOW_MINUS_ONE))
        m_x = x;
    if (y != -1 || (sizeFlags & wxSIZE_ALLOW_MINUS_ONE))
        m_y = y;
    if (width != -1)
        m_width = width;
    if (height != -1)
        m_height = height;

    ConstrainSize();

    if (m_x != old_x || m_y != old_y)
    {
        gtk_window_move(GTK_WINDOW(m_widget), m_x, m_y);
        wxMoveEvent event(wxPoint(m_x, m_y), GetId());
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }

    if ((m_width != old_width) || (m_height != old_height))
    {
        // gtk_window_resize does not work for GTK_WINDOW_POPUP
        gtk_widget_set_size_request( m_widget, m_width, m_height );
        wxSizeEvent event(GetSize(), GetId());
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }
}

void wxPopupWindow::SetFocus()
{
    // set the focus to the first child who wants it
    wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
    while ( node )
    {
        wxWindow *child = node->GetData();
        node = node->GetNext();

        if ( child->CanAcceptFocus() && !child->IsTopLevel() )
        {
            child->SetFocus();
            return;
        }
    }

    wxPopupWindowBase::SetFocus();
}

bool wxPopupWindow::Show( bool show )
{
    if (show && !IsShown())
    {
        wxSizeEvent event(GetSize(), GetId());
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }

    bool ret = wxWindow::Show( show );

    return ret;
}

#endif // wxUSE_POPUPWIN
