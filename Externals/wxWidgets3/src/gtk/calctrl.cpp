///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/calctrl.cpp
// Purpose:     implementation of the wxGtkCalendarCtrl
// Author:      Marcin Wojdyr
// Copyright:   (c) 2008 Marcin Wojdyr
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_CALENDARCTRL

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#include "wx/calctrl.h"

#include <gtk/gtk.h>

extern "C" {

static void gtk_day_selected_callback(GtkWidget *WXUNUSED(widget),
                                      wxGtkCalendarCtrl *cal)
{
    cal->GTKGenerateEvent(wxEVT_CALENDAR_SEL_CHANGED);
}

static void gtk_day_selected_double_click_callback(GtkWidget *WXUNUSED(widget),
                                                   wxGtkCalendarCtrl *cal)
{
    cal->GTKGenerateEvent(wxEVT_CALENDAR_DOUBLECLICKED);
}

static void gtk_month_changed_callback(GtkWidget *WXUNUSED(widget),
                                       wxGtkCalendarCtrl *cal)
{
    cal->GTKGenerateEvent(wxEVT_CALENDAR_PAGE_CHANGED);
}

// callbacks that send deprecated events

static void gtk_prev_month_callback(GtkWidget *WXUNUSED(widget),
                                    wxGtkCalendarCtrl *cal)
{
    cal->GTKGenerateEvent(wxEVT_CALENDAR_MONTH_CHANGED);
}

static void gtk_prev_year_callback(GtkWidget *WXUNUSED(widget),
                                    wxGtkCalendarCtrl *cal)
{
    cal->GTKGenerateEvent(wxEVT_CALENDAR_YEAR_CHANGED);
}

}

// ----------------------------------------------------------------------------
// wxGtkCalendarCtrl
// ----------------------------------------------------------------------------


bool wxGtkCalendarCtrl::Create(wxWindow *parent,
                               wxWindowID id,
                               const wxDateTime& date,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style,
                               const wxString& name)
{
    if (!PreCreation(parent, pos, size) ||
          !CreateBase(parent, id, pos, size, style, wxDefaultValidator, name))
    {
        wxFAIL_MSG(wxT("wxGtkCalendarCtrl creation failed"));
        return false;
    }

    m_widget = gtk_calendar_new();
    g_object_ref(m_widget);
    SetDate(date.IsValid() ? date : wxDateTime::Today());

    if (style & wxCAL_NO_MONTH_CHANGE)
        g_object_set (G_OBJECT (m_widget), "no-month-change", true, NULL);
    if (style & wxCAL_SHOW_WEEK_NUMBERS)
        g_object_set (G_OBJECT (m_widget), "show-week-numbers", true, NULL);

    g_signal_connect_after(m_widget, "day-selected",
                           G_CALLBACK (gtk_day_selected_callback),
                           this);
    g_signal_connect_after(m_widget, "day-selected-double-click",
                           G_CALLBACK (gtk_day_selected_double_click_callback),
                           this);
    g_signal_connect_after(m_widget, "month-changed",
                           G_CALLBACK (gtk_month_changed_callback),
                           this);

    // connect callbacks that send deprecated events
    g_signal_connect_after(m_widget, "prev-month",
                           G_CALLBACK (gtk_prev_month_callback),
                           this);
    g_signal_connect_after(m_widget, "next-month",
                           G_CALLBACK (gtk_prev_month_callback),
                           this);
    g_signal_connect_after(m_widget, "prev-year",
                           G_CALLBACK (gtk_prev_year_callback),
                           this);
    g_signal_connect_after(m_widget, "next-year",
                           G_CALLBACK (gtk_prev_year_callback),
                           this);

    m_parent->DoAddChild(this);

    PostCreation(size);

    return true;
}

void wxGtkCalendarCtrl::GTKGenerateEvent(wxEventType type)
{
    // First check if the new date is in the specified range.
    wxDateTime dt = GetDate();
    if ( !IsInValidRange(dt) )
    {
        if ( m_validStart.IsValid() && dt < m_validStart )
            dt = m_validStart;
        else
            dt = m_validEnd;

        SetDate(dt);

        return;
    }

    if ( type == wxEVT_CALENDAR_SEL_CHANGED )
    {
        // Don't generate this event if the new date is the same as the old
        // one.
        if ( m_selectedDate == dt )
            return;

        m_selectedDate = dt;

        GenerateEvent(type);

        // Also send the deprecated event together with the new one.
        GenerateEvent(wxEVT_CALENDAR_DAY_CHANGED);
    }
    else
    {
        GenerateEvent(type);
    }
}

bool wxGtkCalendarCtrl::IsInValidRange(const wxDateTime& dt) const
{
    return (!m_validStart.IsValid() || m_validStart <= dt) &&
                (!m_validEnd.IsValid() || dt <= m_validEnd);
}

bool
wxGtkCalendarCtrl::SetDateRange(const wxDateTime& lowerdate,
                                const wxDateTime& upperdate)
{
    if ( lowerdate.IsValid() && upperdate.IsValid() && lowerdate >= upperdate )
        return false;

    m_validStart = lowerdate;
    m_validEnd = upperdate;

    return true;
}

bool
wxGtkCalendarCtrl::GetDateRange(wxDateTime *lowerdate,
                                wxDateTime *upperdate) const
{
    if ( lowerdate )
        *lowerdate = m_validStart;
    if ( upperdate )
        *upperdate = m_validEnd;

    return m_validStart.IsValid() || m_validEnd.IsValid();
}


bool wxGtkCalendarCtrl::EnableMonthChange(bool enable)
{
    if ( !wxCalendarCtrlBase::EnableMonthChange(enable) )
        return false;

    g_object_set (G_OBJECT (m_widget), "no-month-change", !enable, NULL);

    return true;
}


bool wxGtkCalendarCtrl::SetDate(const wxDateTime& date)
{
    if ( date.IsValid() && !IsInValidRange(date) )
        return false;

    g_signal_handlers_block_by_func(m_widget,
        (gpointer) gtk_day_selected_callback, this);
    g_signal_handlers_block_by_func(m_widget,
        (gpointer) gtk_month_changed_callback, this);

    m_selectedDate = date;
    int year = date.GetYear();
    int month = date.GetMonth();
    int day = date.GetDay();
    gtk_calendar_select_month(GTK_CALENDAR(m_widget), month, year);
    gtk_calendar_select_day(GTK_CALENDAR(m_widget), day);

    g_signal_handlers_unblock_by_func( m_widget,
        (gpointer) gtk_month_changed_callback, this);
    g_signal_handlers_unblock_by_func( m_widget,
        (gpointer) gtk_day_selected_callback, this);

    return true;
}

wxDateTime wxGtkCalendarCtrl::GetDate() const
{
    guint year, monthGTK, day;
    gtk_calendar_get_date(GTK_CALENDAR(m_widget), &year, &monthGTK, &day);

    // GTK may return an invalid date, this happens at least when switching the
    // month (or the year in case of February in a leap year) and the new month
    // has fewer days than the currently selected one making the currently
    // selected day invalid, e.g. just choosing May 31 and going back a month
    // results in the date being (non existent) April 31 when we're called from
    // gtk_prev_month_callback(). We need to manually work around this to avoid
    // asserts from wxDateTime ctor.
    const wxDateTime::Month month = static_cast<wxDateTime::Month>(monthGTK);
    const guint dayMax = wxDateTime::GetNumberOfDays(month, year);
    if ( day > dayMax )
        day = dayMax;

    return wxDateTime(day, month, year);
}

void wxGtkCalendarCtrl::Mark(size_t day, bool mark)
{
    if (mark)
        gtk_calendar_mark_day(GTK_CALENDAR(m_widget), day);
    else
        gtk_calendar_unmark_day(GTK_CALENDAR(m_widget), day);
}

#endif // wxUSE_CALENDARCTRL
