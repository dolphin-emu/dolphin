/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/scrolbar.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: scrolbar.cpp 66555 2011-01-04 08:31:53Z SC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_SCROLLBAR

#include "wx/scrolbar.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
#endif

#include "wx/gtk/private.h"

//-----------------------------------------------------------------------------
// "value_changed" from scrollbar
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_value_changed(GtkRange* range, wxScrollBar* win)
{
    wxEventType eventType = win->GTKGetScrollEventType(range);
    if (eventType != wxEVT_NULL)
    {
        const int orient = win->HasFlag(wxSB_VERTICAL) ? wxVERTICAL : wxHORIZONTAL;
        const int value = win->GetThumbPosition();
        const int id = win->GetId();

        // first send the specific event for the user action
        wxScrollEvent evtSpec(eventType, id, value, orient);
        evtSpec.SetEventObject(win);
        win->HandleWindowEvent(evtSpec);

        if (!win->m_isScrolling)
        {
            // and if it's over also send a general "changed" event
            wxScrollEvent evtChanged(wxEVT_SCROLL_CHANGED, id, value, orient);
            evtChanged.SetEventObject(win);
            win->HandleWindowEvent(evtChanged);
        }
    }
}
}

//-----------------------------------------------------------------------------
// "button_press_event" from scrollbar
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_button_press_event(GtkRange*, GdkEventButton*, wxScrollBar* win)
{
    win->m_mouseButtonDown = true;
    return false;
}
}

//-----------------------------------------------------------------------------
// "event_after" from scrollbar
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_event_after(GtkRange* range, GdkEvent* event, wxScrollBar* win)
{
    if (event->type == GDK_BUTTON_RELEASE)
    {
        g_signal_handlers_block_by_func(range, (void*)gtk_event_after, win);

        const int value = win->GetThumbPosition();
        const int orient = win->HasFlag(wxSB_VERTICAL) ? wxVERTICAL : wxHORIZONTAL;
        const int id = win->GetId();

        wxScrollEvent evtRel(wxEVT_SCROLL_THUMBRELEASE, id, value, orient);
        evtRel.SetEventObject(win);
        win->HandleWindowEvent(evtRel);

        wxScrollEvent evtChanged(wxEVT_SCROLL_CHANGED, id, value, orient);
        evtChanged.SetEventObject(win);
        win->HandleWindowEvent(evtChanged);
    }
}
}

//-----------------------------------------------------------------------------
// "button_release_event" from scrollbar
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_button_release_event(GtkRange* range, GdkEventButton*, wxScrollBar* win)
{
    win->m_mouseButtonDown = false;
    // If thumb tracking
    if (win->m_isScrolling)
    {
        win->m_isScrolling = false;
        // Hook up handler to send thumb release event after this emission is finished.
        // To allow setting scroll position from event handler, sending event must
        // be deferred until after the GtkRange handler for this signal has run
        g_signal_handlers_unblock_by_func(range, (void*)gtk_event_after, win);
    }

    return false;
}
}

//-----------------------------------------------------------------------------
// wxScrollBar
//-----------------------------------------------------------------------------

wxScrollBar::wxScrollBar()
{
}

wxScrollBar::~wxScrollBar()
{
}

bool wxScrollBar::Create(wxWindow *parent, wxWindowID id,
           const wxPoint& pos, const wxSize& size,
           long style, const wxValidator& validator, const wxString& name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxScrollBar creation failed") );
        return false;
    }

    const bool isVertical = (style & wxSB_VERTICAL) != 0;
    if (isVertical)
        m_widget = gtk_vscrollbar_new( NULL );
    else
        m_widget = gtk_hscrollbar_new( NULL );
    g_object_ref(m_widget);

    m_scrollBar[0] = (GtkRange*)m_widget;

    g_signal_connect_after(m_widget, "value_changed",
                     G_CALLBACK(gtk_value_changed), this);
    g_signal_connect(m_widget, "button_press_event",
                     G_CALLBACK(gtk_button_press_event), this);
    g_signal_connect(m_widget, "button_release_event",
                     G_CALLBACK(gtk_button_release_event), this);

    gulong handler_id;
    handler_id = g_signal_connect(
        m_widget, "event_after", G_CALLBACK(gtk_event_after), this);
    g_signal_handler_block(m_widget, handler_id);

    m_parent->DoAddChild( this );

    PostCreation(size);

    return true;
}

int wxScrollBar::GetThumbPosition() const
{
    GtkAdjustment* adj = ((GtkRange*)m_widget)->adjustment;
    return int(adj->value + 0.5);
}

int wxScrollBar::GetThumbSize() const
{
    GtkAdjustment* adj = ((GtkRange*)m_widget)->adjustment;
    return int(adj->page_size);
}

int wxScrollBar::GetPageSize() const
{
    GtkAdjustment* adj = ((GtkRange*)m_widget)->adjustment;
    return int(adj->page_increment);
}

int wxScrollBar::GetRange() const
{
    GtkAdjustment* adj = ((GtkRange*)m_widget)->adjustment;
    return int(adj->upper);
}

void wxScrollBar::SetThumbPosition( int viewStart )
{
    if (GetThumbPosition() != viewStart)
    {
        g_signal_handlers_block_by_func(m_widget,
            (gpointer)gtk_value_changed, this);

        gtk_range_set_value((GtkRange*)m_widget, viewStart);
        m_scrollPos[0] = gtk_range_get_value((GtkRange*)m_widget);

        g_signal_handlers_unblock_by_func(m_widget,
            (gpointer)gtk_value_changed, this);
    }
}

void wxScrollBar::SetScrollbar(int position, int thumbSize, int range, int pageSize, bool)
{
    if (range == 0)
    {
        // GtkRange requires upper > lower
        range =
        thumbSize = 1;
    }
    GtkAdjustment* adj = ((GtkRange*)m_widget)->adjustment;
    adj->step_increment = 1;
    adj->page_increment = pageSize;
    adj->page_size = thumbSize;
    adj->value = position;
    g_signal_handlers_block_by_func(m_widget, (void*)gtk_value_changed, this);
    gtk_range_set_range((GtkRange*)m_widget, 0, range);
    m_scrollPos[0] = adj->value;
    g_signal_handlers_unblock_by_func(m_widget, (void*)gtk_value_changed, this);
}

void wxScrollBar::SetPageSize( int pageLength )
{
    SetScrollbar(GetThumbPosition(), GetThumbSize(), GetRange(), pageLength);
}

void wxScrollBar::SetRange(int range)
{
    SetScrollbar(GetThumbPosition(), GetThumbSize(), range, GetPageSize());
}

GdkWindow *wxScrollBar::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return m_widget->window;
}

// static
wxVisualAttributes
wxScrollBar::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_vscrollbar_new);
}

#endif // wxUSE_SCROLLBAR
