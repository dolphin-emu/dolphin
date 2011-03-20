/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/slider.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: slider.cpp 66555 2011-01-04 08:31:53Z SC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_SLIDER

#include "wx/slider.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/math.h"
#endif

#include <gtk/gtk.h>

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

extern bool g_blockEventsOnDrag;

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

// process a scroll event
static void
ProcessScrollEvent(wxSlider *win, wxEventType evtType)
{
    const int orient = win->HasFlag(wxSL_VERTICAL) ? wxVERTICAL
                                                   : wxHORIZONTAL;

    const int value = win->GetValue();

    // if we have any "special" event (i.e. the value changed by a line or a
    // page), send this specific event first
    if ( evtType != wxEVT_NULL )
    {
        wxScrollEvent event( evtType, win->GetId(), value, orient );
        event.SetEventObject( win );
        win->HandleWindowEvent( event );
    }

    // but, in any case, except if we're dragging the slider (and so the change
    // is not definitive), send a generic "changed" event
    if ( evtType != wxEVT_SCROLL_THUMBTRACK )
    {
        wxScrollEvent event(wxEVT_SCROLL_CHANGED, win->GetId(), value, orient);
        event.SetEventObject( win );
        win->HandleWindowEvent( event );
    }

    // and also generate a command event for compatibility
    wxCommandEvent event( wxEVT_COMMAND_SLIDER_UPDATED, win->GetId() );
    event.SetEventObject( win );
    event.SetInt( value );
    win->HandleWindowEvent( event );
}

static inline wxEventType GtkScrollTypeToWx(int scrollType)
{
    wxEventType eventType;
    switch (scrollType)
    {
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_LEFT:
    case GTK_SCROLL_STEP_UP:
        eventType = wxEVT_SCROLL_LINEUP;
        break;
    case GTK_SCROLL_STEP_DOWN:
    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_STEP_RIGHT:
        eventType = wxEVT_SCROLL_LINEDOWN;
        break;
    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_LEFT:
    case GTK_SCROLL_PAGE_UP:
        eventType = wxEVT_SCROLL_PAGEUP;
        break;
    case GTK_SCROLL_PAGE_DOWN:
    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_PAGE_RIGHT:
        eventType = wxEVT_SCROLL_PAGEDOWN;
        break;
    case GTK_SCROLL_START:
        eventType = wxEVT_SCROLL_TOP;
        break;
    case GTK_SCROLL_END:
        eventType = wxEVT_SCROLL_BOTTOM;
        break;
    case GTK_SCROLL_JUMP:
        eventType = wxEVT_SCROLL_THUMBTRACK;
        break;
    default:
        wxFAIL_MSG(wxT("Unknown GtkScrollType"));
        eventType = wxEVT_NULL;
        break;
    }
    return eventType;
}

// Determine if increment is the same as +/-x, allowing for some small
//   difference due to possible inexactness in floating point arithmetic
static inline bool IsScrollIncrement(double increment, double x)
{
    wxASSERT(increment > 0);
    const double tolerance = 1.0 / 1024;
    return fabs(increment - fabs(x)) < tolerance;
}

//-----------------------------------------------------------------------------
// "value_changed"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_value_changed(GtkRange* range, wxSlider* win)
{
    GtkAdjustment* adj = gtk_range_get_adjustment (range);
    const int pos = wxRound(adj->value);
    const double oldPos = win->m_pos;
    win->m_pos = adj->value;

    if (!win->m_hasVMT || g_blockEventsOnDrag)
        return;

    if (win->GTKEventsDisabled())
    {
        win->m_scrollEventType = GTK_SCROLL_NONE;
        return;
    }

    wxEventType eventType = wxEVT_NULL;
    if (win->m_isScrolling)
    {
        eventType = wxEVT_SCROLL_THUMBTRACK;
    }
    else if (win->m_scrollEventType != GTK_SCROLL_NONE)
    {
        // Scroll event from "move-slider" (keyboard)
        eventType = GtkScrollTypeToWx(win->m_scrollEventType);
    }
    else if (win->m_mouseButtonDown)
    {
        // Difference from last change event
        const double diff = adj->value - oldPos;
        const bool isDown = diff > 0;

        if (IsScrollIncrement(adj->page_increment, diff))
        {
            eventType = isDown ? wxEVT_SCROLL_PAGEDOWN : wxEVT_SCROLL_PAGEUP;
        }
        else if (wxIsSameDouble(adj->value, 0))
        {
            eventType = wxEVT_SCROLL_PAGEUP;
        }
        else if (wxIsSameDouble(adj->value, adj->upper))
        {
            eventType = wxEVT_SCROLL_PAGEDOWN;
        }
        else
        {
            // Assume track event
            eventType = wxEVT_SCROLL_THUMBTRACK;
            // Remember that we're tracking
            win->m_isScrolling = true;
        }
    }

    win->m_scrollEventType = GTK_SCROLL_NONE;

    // If integral position has changed
    if (wxRound(oldPos) != pos)
    {
        ProcessScrollEvent(win, eventType);
        win->m_needThumbRelease = eventType == wxEVT_SCROLL_THUMBTRACK;
    }
}
}

//-----------------------------------------------------------------------------
// "move_slider" (keyboard event)
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_move_slider(GtkRange*, GtkScrollType scrollType, wxSlider* win)
{
    // Save keyboard scroll type for "value_changed" handler
    win->m_scrollEventType = scrollType;
}
}

//-----------------------------------------------------------------------------
// "button_press_event"
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_button_press_event(GtkWidget*, GdkEventButton*, wxSlider* win)
{
    win->m_mouseButtonDown = true;

    return false;
}
}

//-----------------------------------------------------------------------------
// "event_after"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_event_after(GtkRange* range, GdkEvent* event, wxSlider* win)
{
    if (event->type == GDK_BUTTON_RELEASE)
    {
        g_signal_handlers_block_by_func(range, (gpointer) gtk_event_after, win);

        if (win->m_needThumbRelease)
        {
            win->m_needThumbRelease = false;
            ProcessScrollEvent(win, wxEVT_SCROLL_THUMBRELEASE);
        }
        // Keep slider at an integral position
        win->GTKDisableEvents();
        gtk_range_set_value(GTK_RANGE (win->m_scale), win->GetValue());
        win->GTKEnableEvents();
    }
}
}

//-----------------------------------------------------------------------------
// "button_release_event"
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_button_release_event(GtkRange* range, GdkEventButton*, wxSlider* win)
{
    win->m_mouseButtonDown = false;
    if (win->m_isScrolling)
    {
        win->m_isScrolling = false;
        g_signal_handlers_unblock_by_func(range, (gpointer) gtk_event_after, win);
    }
    return false;
}
}

//-----------------------------------------------------------------------------
// "format_value"
//-----------------------------------------------------------------------------

extern "C" {
static gchar* gtk_format_value(GtkScale*, double value, void*)
{
    // Format value as nearest integer
    return g_strdup_printf("%d", wxRound(value));
}
}

//-----------------------------------------------------------------------------
// wxSlider
//-----------------------------------------------------------------------------

wxSlider::wxSlider()
{
    m_pos = 0;
    m_scrollEventType = GTK_SCROLL_NONE;
    m_needThumbRelease = false;
    m_blockScrollEvent = false;
}

bool wxSlider::Create(wxWindow *parent,
                      wxWindowID id,
                      int value,
                      int minValue,
                      int maxValue,
                      const wxPoint& pos,
                      const wxSize& size,
                      long style,
                      const wxValidator& validator,
                      const wxString& name)
{
    m_pos = value;
    m_scrollEventType = GTK_SCROLL_NONE;

    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxSlider creation failed") );
        return false;
    }


    if (style & wxSL_VERTICAL)
        m_scale = gtk_vscale_new( NULL );
    else
        m_scale = gtk_hscale_new( NULL );
    g_object_ref(m_scale);

    if (style & wxSL_MIN_MAX_LABELS)
    {
        gtk_widget_show( m_scale );

        if (style & wxSL_VERTICAL)
            m_widget = gtk_hbox_new(false, 0);
        else
            m_widget = gtk_vbox_new(false, 0);
        g_object_ref(m_widget);
        gtk_widget_show( m_widget );
        gtk_container_add( GTK_CONTAINER(m_widget), m_scale );

        GtkWidget *box;
        if (style & wxSL_VERTICAL)
            box = gtk_vbox_new(false,0);
        else
            box = gtk_hbox_new(false,0);
        g_object_ref(box);
        gtk_widget_show(box);
        gtk_container_add( GTK_CONTAINER(m_widget), box );

        m_minLabel = gtk_label_new(NULL);
        g_object_ref(m_minLabel);
        gtk_widget_show( m_minLabel );
        gtk_container_add( GTK_CONTAINER(box), m_minLabel );
        gtk_box_set_child_packing( GTK_BOX(box), m_minLabel, FALSE, FALSE, 0, GTK_PACK_START );

        // expanding empty space between the min/max labels
        GtkWidget *space = gtk_label_new(NULL);
        g_object_ref(space);
        gtk_widget_show( space );
        gtk_container_add( GTK_CONTAINER(box), space );
        gtk_box_set_child_packing( GTK_BOX(box), space, TRUE, FALSE, 0, GTK_PACK_START );

        m_maxLabel = gtk_label_new(NULL);
        g_object_ref(m_maxLabel);
        gtk_widget_show( m_maxLabel );
        gtk_container_add( GTK_CONTAINER(box), m_maxLabel );
        gtk_box_set_child_packing( GTK_BOX(box), m_maxLabel, FALSE, FALSE, 0, GTK_PACK_END );
    }
    else
    {
        m_widget = m_scale;
        m_maxLabel = NULL;
        m_minLabel = NULL;
    }

    const bool showValueLabel = (style & wxSL_VALUE_LABEL) != 0;
    gtk_scale_set_draw_value(GTK_SCALE (m_scale), showValueLabel );
    if ( showValueLabel )
    {
        // position the label appropriately: notice that wxSL_DIRECTION flags
        // specify the position of the ticks, not label, under MSW and so the
        // label is on the opposite side
        GtkPositionType posLabel;
        if ( style & wxSL_VERTICAL )
        {
            if ( style & wxSL_LEFT )
                posLabel = GTK_POS_RIGHT;
            else // if ( style & wxSL_RIGHT ) -- this is also the default
                posLabel = GTK_POS_LEFT;
        }
        else // horizontal slider
        {
            if ( style & wxSL_TOP )
                posLabel = GTK_POS_BOTTOM;
            else // if ( style & wxSL_BOTTOM) -- this is again the default
                posLabel = GTK_POS_TOP;
        }

        gtk_scale_set_value_pos( GTK_SCALE(m_scale), posLabel );
    }

    // Keep full precision in position value
    gtk_scale_set_digits(GTK_SCALE (m_scale), -1);

    if (style & wxSL_INVERSE)
        gtk_range_set_inverted( GTK_RANGE(m_scale), TRUE );

    g_signal_connect(m_scale, "button_press_event", G_CALLBACK(gtk_button_press_event), this);
    g_signal_connect(m_scale, "button_release_event", G_CALLBACK(gtk_button_release_event), this);
    g_signal_connect(m_scale, "move_slider", G_CALLBACK(gtk_move_slider), this);
    g_signal_connect(m_scale, "format_value", G_CALLBACK(gtk_format_value), NULL);
    g_signal_connect(m_scale, "value_changed", G_CALLBACK(gtk_value_changed), this);
    gulong handler_id = g_signal_connect(m_scale, "event_after", G_CALLBACK(gtk_event_after), this);
    g_signal_handler_block(m_scale, handler_id);

    SetRange( minValue, maxValue );

    // don't call the public SetValue() as it won't do anything unless the
    // value really changed
    GTKSetValue( value );

    m_parent->DoAddChild( this );

    PostCreation(size);

    return true;
}

void wxSlider::GTKDisableEvents()
{
    m_blockScrollEvent = true;
}

void wxSlider::GTKEnableEvents()
{
    m_blockScrollEvent = false;
}

bool wxSlider::GTKEventsDisabled() const
{
   return m_blockScrollEvent;
}

int wxSlider::GetValue() const
{
    return wxRound(m_pos);
}

void wxSlider::SetValue( int value )
{
    if (GetValue() != value)
        GTKSetValue(value);
}

void wxSlider::GTKSetValue(int value)
{
    GTKDisableEvents();
    gtk_range_set_value(GTK_RANGE (m_scale), value);
    GTKEnableEvents();
}

void wxSlider::SetRange( int minValue, int maxValue )
{
    GTKDisableEvents();
    if (minValue == maxValue)
       maxValue++;
    gtk_range_set_range(GTK_RANGE (m_scale), minValue, maxValue);
    gtk_range_set_increments(GTK_RANGE (m_scale), 1, (maxValue - minValue + 9) / 10);
    GTKEnableEvents();

    if (HasFlag(wxSL_MIN_MAX_LABELS))
    {
        wxString str;

        str.Printf( "%d", minValue );
        if (HasFlag(wxSL_INVERSE))
            gtk_label_set_text( GTK_LABEL(m_maxLabel), str.utf8_str() );
        else
            gtk_label_set_text( GTK_LABEL(m_minLabel), str.utf8_str() );

        str.Printf( "%d", maxValue );
        if (HasFlag(wxSL_INVERSE))
            gtk_label_set_text( GTK_LABEL(m_minLabel), str.utf8_str() );
        else
            gtk_label_set_text( GTK_LABEL(m_maxLabel), str.utf8_str() );

    }
}

int wxSlider::GetMin() const
{
    return int(gtk_range_get_adjustment (GTK_RANGE (m_scale))->lower);
}

int wxSlider::GetMax() const
{
    return int(gtk_range_get_adjustment (GTK_RANGE (m_scale))->upper);
}

void wxSlider::SetPageSize( int pageSize )
{
    GTKDisableEvents();
    gtk_range_set_increments(GTK_RANGE (m_scale), GetLineSize(), pageSize);
    GTKEnableEvents();
}

int wxSlider::GetPageSize() const
{
    return int(gtk_range_get_adjustment (GTK_RANGE (m_scale))->page_increment);
}

// GTK does not support changing the size of the slider
void wxSlider::SetThumbLength(int)
{
}

int wxSlider::GetThumbLength() const
{
    return 0;
}

void wxSlider::SetLineSize( int lineSize )
{
    GTKDisableEvents();
    gtk_range_set_increments(GTK_RANGE (m_scale), lineSize, GetPageSize());
    GTKEnableEvents();
}

int wxSlider::GetLineSize() const
{
    return int(gtk_range_get_adjustment (GTK_RANGE (m_scale))->step_increment);
}

GdkWindow *wxSlider::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return GTK_RANGE(m_scale)->event_window;
}

// static
wxVisualAttributes
wxSlider::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_vscale_new);
}

#endif // wxUSE_SLIDER
