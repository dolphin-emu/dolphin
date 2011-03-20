/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/frame.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: frame.cpp 66648 2011-01-08 06:42:41Z PC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/frame.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/toolbar.h"
    #include "wx/statusbr.h"
#endif // WX_PRECOMP

#include <gtk/gtk.h>

#if wxUSE_LIBHILDON
    #include <hildon-widgets/hildon-window.h>
#endif // wxUSE_LIBHILDON

#if wxUSE_LIBHILDON2
    #include <hildon/hildon.h>
#endif // wxUSE_LIBHILDON2

// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFrame creation
// ----------------------------------------------------------------------------

void wxFrame::Init()
{
    m_fsSaveFlag = 0;
}

bool wxFrame::Create( wxWindow *parent,
                      wxWindowID id,
                      const wxString& title,
                      const wxPoint& pos,
                      const wxSize& sizeOrig,
                      long style,
                      const wxString &name )
{
    return wxFrameBase::Create(parent, id, title, pos, sizeOrig, style, name);
}

wxFrame::~wxFrame()
{
    SendDestroyEvent();

    DeleteAllBars();
}

// ----------------------------------------------------------------------------
// overridden wxWindow methods
// ----------------------------------------------------------------------------

void wxFrame::DoGetClientSize( int *width, int *height ) const
{
    wxASSERT_MSG( (m_widget != NULL), wxT("invalid frame") );

    wxFrameBase::DoGetClientSize(width, height);

    if (height)
    {
#if wxUSE_MENUS_NATIVE
        // menu bar
        if (m_frameMenuBar && m_frameMenuBar->IsShown())
        {
            GtkRequisition req;
            gtk_widget_size_request(m_frameMenuBar->m_widget, &req);
#if !wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
            *height -= req.height;
#endif
        }
#endif // wxUSE_MENUS_NATIVE

#if wxUSE_STATUSBAR
        // status bar
        if (m_frameStatusBar && m_frameStatusBar->IsShown())
            *height -= m_frameStatusBar->m_height;
#endif // wxUSE_STATUSBAR
    }

#if wxUSE_TOOLBAR
    // tool bar
    if (m_frameToolBar && m_frameToolBar->IsShown())
    {
        GtkRequisition req;
        gtk_widget_size_request(m_frameToolBar->m_widget, &req);
        if (m_frameToolBar->IsVertical())
        {
            if (width)
                *width -= req.width;
        }
        else
        {
            if (height)
                *height -= req.height;
        }
    }
#endif // wxUSE_TOOLBAR

    if (width != NULL && *width < 0)
        *width = 0;
    if (height != NULL && *height < 0)
        *height = 0;
}

#if wxUSE_MENUS && wxUSE_ACCEL
// Helper for wxCreateAcceleratorTableForMenuBar
static void wxAddAccelerators(wxList& accelEntries, wxMenu* menu)
{
    size_t i;
    for (i = 0; i < menu->GetMenuItems().GetCount(); i++)
    {
        wxMenuItem* item = (wxMenuItem*) menu->GetMenuItems().Item(i)->GetData();
        if (item->GetSubMenu())
        {
            wxAddAccelerators(accelEntries, item->GetSubMenu());
        }
        else if (!item->GetItemLabel().IsEmpty())
        {
            wxAcceleratorEntry* entry = wxAcceleratorEntry::Create(item->GetItemLabel());
            if (entry)
            {
                entry->Set(entry->GetFlags(), entry->GetKeyCode(), item->GetId());
                accelEntries.Append((wxObject*) entry);
            }
        }
    }
}

// Create an accelerator table consisting of all the accelerators
// from the menubar in the given menus
static wxAcceleratorTable wxCreateAcceleratorTableForMenuBar(wxMenuBar* menuBar)
{
    wxList accelEntries;

    size_t i;
    for (i = 0; i < menuBar->GetMenuCount(); i++)
    {
        wxAddAccelerators(accelEntries, menuBar->GetMenu(i));
    }

    size_t n = accelEntries.GetCount();

    if (n == 0)
        return wxAcceleratorTable();

    wxAcceleratorEntry* entries = new wxAcceleratorEntry[n];

    for (i = 0; i < accelEntries.GetCount(); i++)
    {
        wxAcceleratorEntry* entry = (wxAcceleratorEntry*) accelEntries.Item(i)->GetData();
        entries[i] = (*entry);
        delete entry;

    }

    wxAcceleratorTable table(n, entries);
    delete[] entries;

    return table;
}
#endif // wxUSE_MENUS && wxUSE_ACCEL

bool wxFrame::ShowFullScreen(bool show, long style)
{
    if (!wxFrameBase::ShowFullScreen(show, style))
        return false;

#if wxUSE_MENUS && wxUSE_ACCEL
    if (show && GetMenuBar())
    {
        wxAcceleratorTable table(wxCreateAcceleratorTableForMenuBar(GetMenuBar()));
        if (table.IsOk())
            SetAcceleratorTable(table);
    }
#endif // wxUSE_MENUS && wxUSE_ACCEL

    wxWindow* const bar[] = {
#if wxUSE_MENUS
        m_frameMenuBar,
#else
        NULL,
#endif
#if wxUSE_TOOLBAR
        m_frameToolBar,
#else
        NULL,
#endif
#if wxUSE_STATUSBAR
        m_frameStatusBar,
#else
        NULL,
#endif
    };
    const long fsNoBar[] = {
        wxFULLSCREEN_NOMENUBAR, wxFULLSCREEN_NOTOOLBAR, wxFULLSCREEN_NOSTATUSBAR
    };
    for (int i = 0; i < 3; i++)
    {
        if (show)
        {
            if (bar[i] && (style & fsNoBar[i]))
            {
                if (bar[i]->IsShown())
                    bar[i]->Show(false);
                else
                    style &= ~fsNoBar[i];
            }
        }
        else
        {
            if (bar[i] && (m_fsSaveFlag & fsNoBar[i]))
                bar[i]->Show(true);
        }
    }
    if (show)
        m_fsSaveFlag = style;

    return true;
}

bool wxFrame::SendIdleEvents(wxIdleEvent& event)
{
    bool needMore = wxFrameBase::SendIdleEvents(event);

#if wxUSE_MENUS
    if (m_frameMenuBar && m_frameMenuBar->SendIdleEvents(event))
        needMore = true;
#endif
#if wxUSE_TOOLBAR
    if (m_frameToolBar && m_frameToolBar->SendIdleEvents(event))
        needMore = true;
#endif
#if wxUSE_STATUSBAR
    if (m_frameStatusBar && m_frameStatusBar->SendIdleEvents(event))
        needMore = true;
#endif

    return needMore;
}

// ----------------------------------------------------------------------------
// menu/tool/status bar stuff
// ----------------------------------------------------------------------------

#if wxUSE_MENUS_NATIVE

void wxFrame::DetachMenuBar()
{
    wxASSERT_MSG( (m_widget != NULL), wxT("invalid frame") );
    wxASSERT_MSG( (m_wxwindow != NULL), wxT("invalid frame") );

    if ( m_frameMenuBar )
    {
#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
        hildon_window_set_menu(HILDON_WINDOW(m_widget), NULL);
#else // !wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
        gtk_widget_ref( m_frameMenuBar->m_widget );

        gtk_container_remove( GTK_CONTAINER(m_mainWidget), m_frameMenuBar->m_widget );
#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2 /!wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
    }

    wxFrameBase::DetachMenuBar();

    // make sure next size_allocate causes a wxSizeEvent
    m_oldClientWidth = 0;
}

void wxFrame::AttachMenuBar( wxMenuBar *menuBar )
{
    wxFrameBase::AttachMenuBar(menuBar);

    if (m_frameMenuBar)
    {
#if wxUSE_LIBHILDON || wxUSE_LIBHILDON2
        hildon_window_set_menu(HILDON_WINDOW(m_widget),
                               GTK_MENU(m_frameMenuBar->m_menubar));
#else // !wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
        m_frameMenuBar->SetParent(this);

        // menubar goes into top of vbox (m_mainWidget)
        gtk_box_pack_start(
            GTK_BOX(m_mainWidget), menuBar->m_widget, false, false, 0);
        gtk_box_reorder_child(GTK_BOX(m_mainWidget), menuBar->m_widget, 0);

        // disconnect wxWindowGTK "size_request" handler,
        // it interferes with sizing of detached GtkHandleBox
        gulong handler_id = g_signal_handler_find(
            menuBar->m_widget,
            GSignalMatchType(G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA),
            g_signal_lookup("size_request", GTK_TYPE_WIDGET),
            0, NULL, NULL, menuBar);
        if (handler_id != 0)
            g_signal_handler_disconnect(menuBar->m_widget, handler_id);

        // reset size request to allow native sizing to work
        gtk_widget_set_size_request(menuBar->m_widget, -1, -1);

        gtk_widget_show( m_frameMenuBar->m_widget );
#endif // wxUSE_LIBHILDON || wxUSE_LIBHILDON2/!wxUSE_LIBHILDON && !wxUSE_LIBHILDON2
    }
    // make sure next size_allocate causes a wxSizeEvent
    m_oldClientWidth = 0;
}
#endif // wxUSE_MENUS_NATIVE

#if wxUSE_TOOLBAR

void wxFrame::SetToolBar(wxToolBar *toolbar)
{
    m_frameToolBar = toolbar;
    if (toolbar)
    {
        if (toolbar->IsVertical())
        {
            // Vertical toolbar and m_wxwindow go into an hbox, inside the
            // vbox (m_mainWidget). hbox is created on demand.
            GtkWidget* hbox = m_wxwindow->parent;
            if (!GTK_IS_HBOX(hbox))
            {
                hbox = gtk_hbox_new(false, 0);
                gtk_widget_show(hbox);
                gtk_container_add(GTK_CONTAINER(m_mainWidget), hbox);
                gtk_widget_reparent(m_wxwindow, hbox);
            }
            gtk_widget_reparent(toolbar->m_widget, hbox);
            gtk_box_set_child_packing(GTK_BOX(hbox),
                toolbar->m_widget, false, false, 0, GTK_PACK_START);

            int pos = 0;  // left
            if (toolbar->HasFlag(wxTB_RIGHT))
                pos = 1;  // right
            gtk_box_reorder_child(GTK_BOX(hbox), toolbar->m_widget, pos);
        }
        else
        {
            // Horizontal toolbar goes into vbox (m_mainWidget)
            gtk_widget_reparent(toolbar->m_widget, m_mainWidget);
            gtk_box_set_child_packing(GTK_BOX(m_mainWidget),
                toolbar->m_widget, false, false, 0, GTK_PACK_START);

            int pos = 0;  // top
            if (m_frameMenuBar)
                pos = 1;  // below menubar
            if (toolbar->HasFlag(wxTB_BOTTOM))
                pos += 2;  // below client area (m_wxwindow)
            gtk_box_reorder_child(
                GTK_BOX(m_mainWidget), toolbar->m_widget, pos);
        }

        // disconnect wxWindowGTK "size_request" handler,
        // it interferes with sizing of detached GtkHandleBox
        gulong handler_id = g_signal_handler_find(
            toolbar->m_widget,
            GSignalMatchType(G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA),
            g_signal_lookup("size_request", GTK_TYPE_WIDGET),
            0, NULL, NULL, toolbar);
        if (handler_id != 0)
            g_signal_handler_disconnect(toolbar->m_widget, handler_id);

        // reset size request to allow native sizing to work
        gtk_widget_set_size_request(toolbar->m_widget, -1, -1);
    }
    // make sure next size_allocate causes a wxSizeEvent
    m_oldClientWidth = 0;
}

#endif // wxUSE_TOOLBAR

#if wxUSE_STATUSBAR

void wxFrame::SetStatusBar(wxStatusBar *statbar)
{
    m_frameStatusBar = statbar;
    if (statbar)
    {
        // statusbar goes into bottom of vbox (m_mainWidget)
        gtk_widget_reparent(statbar->m_widget, m_mainWidget);
        gtk_box_set_child_packing(GTK_BOX(m_mainWidget),
            statbar->m_widget, false, false, 0, GTK_PACK_END);
        // make sure next size_allocate on statusbar causes a size event
        statbar->m_oldClientWidth = 0;
    }
    // make sure next size_allocate causes a wxSizeEvent
    m_oldClientWidth = 0;
}
#endif // wxUSE_STATUSBAR
