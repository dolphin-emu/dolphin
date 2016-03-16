/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/slider.cpp
// Purpose:
// Author:      Robert Roebling
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
#include "wx/gtk/private/gtk2-compat.h"
#include "wx/gtk/private/eventsdisabler.h"

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
    wxCommandEvent event( wxEVT_SLIDER, win->GetId() );
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
    const double value = gtk_range_get_value(range);
    const double oldPos = win->m_pos;
    win->m_pos = value;

    if (g_blockEventsOnDrag)
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
        const double diff = value - oldPos;
        const bool isDown = diff > 0;

        GtkAdjustment* adj = gtk_range_get_adjustment(range);
        if (IsScrollIncrement(gtk_adjustment_get_page_increment(adj), diff))
        {
            eventType = isDown ? wxEVT_SCROLL_PAGEDOWN : wxEVT_SCROLL_PAGEUP;
        }
        else if (wxIsSameDouble(value, 0))
        {
            eventType = wxEVT_SCROLL_PAGEUP;
        }
        else if (wxIsSameDouble(value, gtk_adjustment_get_upper(adj)))
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
    if (wxRound(oldPos) != wxRound(value))
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
        wxGtkEventsDisabler<wxSlider> noEvents(win);
        gtk_range_set_value(GTK_RANGE (win->m_scale), win->GetValue());
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
    m_scale = NULL;
}

wxSlider::~wxSlider()
{
    if (m_scale && m_scale != m_widget)
        GTKDisconnect(m_scale);
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
    m_needThumbRelease = false;
    m_blockScrollEvent = false;

    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxSlider creation failed") );
        return false;
    }

    const bool isVertical = (style & wxSL_VERTICAL) != 0;
    m_scale = gtk_scale_new(GtkOrientation(isVertical), NULL);

    if (style & wxSL_MIN_MAX_LABELS)
    {
        gtk_widget_show( m_scale );

        m_widget = gtk_box_new(GtkOrientation(!isVertical), 0);
        gtk_box_pack_start(GTK_BOX(m_widget), m_scale, true, true, 0);

        GtkWidget* box = gtk_box_new(GtkOrientation(isVertical), 0);
        gtk_widget_show(box);
        gtk_box_pack_start(GTK_BOX(m_widget), box, true, true, 0);

        m_minLabel = gtk_label_new(NULL);
        gtk_widget_show( m_minLabel );
        gtk_box_pack_start(GTK_BOX(box), m_minLabel, false, false, 0);

        // expanding empty space between the min/max labels
        GtkWidget *space = gtk_label_new(NULL);
        gtk_widget_show( space );
        gtk_box_pack_start(GTK_BOX(box), space, true, false, 0);

        m_maxLabel = gtk_label_new(NULL);
        gtk_widget_show( m_maxLabel );
        gtk_box_pack_end(GTK_BOX(box), m_maxLabel, false, false, 0);
    }
    else
    {
        m_widget = m_scale;
        m_maxLabel = NULL;
        m_minLabel = NULL;
    }
    g_object_ref(m_widget);

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
    wxGtkEventsDisabler<wxSlider> noEvents(this);

    gtk_range_set_value(GTK_RANGE (m_scale), value);
    // GTK only updates value label if handle moves at least 1 pixel
    gtk_widget_queue_draw(m_scale);
}

void wxSlider::SetRange( int minValue, int maxValue )
{
    wxGtkEventsDisabler<wxSlider> noEvents(this);
    if (minValue == maxValue)
       maxValue++;
    gtk_range_set_range(GTK_RANGE (m_scale), minValue, maxValue);
    gtk_range_set_increments(GTK_RANGE (m_scale), 1, (maxValue - minValue + 9) / 10);

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
    GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(m_scale));
    return int(gtk_adjustment_get_lower(adj));
}

int wxSlider::GetMax() const
{
    GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(m_scale));
    return int(gtk_adjustment_get_upper(adj));
}

void wxSlider::SetPageSize( int pageSize )
{
    wxGtkEventsDisabler<wxSlider> noEvents(this);
    gtk_range_set_increments(GTK_RANGE (m_scale), GetLineSize(), pageSize);
}

int wxSlider::GetPageSize() const
{
    GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(m_scale));
    return int(gtk_adjustment_get_page_increment(adj));
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
    wxGtkEventsDisabler<wxSlider> noEvents(this);
    gtk_range_set_increments(GTK_RANGE (m_scale), lineSize, GetPageSize());
}

int wxSlider::GetLineSize() const
{
    GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(m_scale));
    return int(gtk_adjustment_get_step_increment(adj));
}

GdkWindow *wxSlider::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
#ifdef __WXGTK3__
    return GTKFindWindow(m_scale);
#else
    return GTK_RANGE(m_scale)->event_window;
#endif
}

// static
wxVisualAttributes
wxSlider::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_scale_new(GTK_ORIENTATION_VERTICAL, NULL));
}

#endif // wxUSE_SLIDER
