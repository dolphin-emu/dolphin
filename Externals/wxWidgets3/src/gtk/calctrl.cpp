///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/calctrl.cpp
// Purpose:     implementation of the wxGtkCalendarCtrl
// Author:      Marcin Wojdyr
// RCS-ID:      $Id: calctrl.cpp 66568 2011-01-04 11:48:14Z VZ $
// Copyright:   (c) 2008 Marcin Wojdyr
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#if wxUSE_CALENDARCTRL

#include "wx/gtk/private.h"
#include "wx/calctrl.h"
#include "wx/gtk/calctrl.h"


extern "C" {

static void gtk_day_selected_callback(GtkWidget *WXUNUSED(widget),
                                      wxGtkCalendarCtrl *cal)
{
    wxDateTime date = cal->GetDate();
    if (cal->m_selectedDate == date)
        return;

    cal->m_selectedDate = date;

    cal->GenerateEvent(wxEVT_CALENDAR_SEL_CHANGED);
    // send deprecated event
    cal->GenerateEvent(wxEVT_CALENDAR_DAY_CHANGED);
}

static void gtk_day_selected_double_click_callback(GtkWidget *WXUNUSED(widget),
                                                   wxGtkCalendarCtrl *cal)
{
    cal->GenerateEvent(wxEVT_CALENDAR_DOUBLECLICKED);
}

static void gtk_month_changed_callback(GtkWidget *WXUNUSED(widget),
                                       wxGtkCalendarCtrl *cal)
{
    cal->GenerateEvent(wxEVT_CALENDAR_PAGE_CHANGED);
}

// callbacks that send deprecated events

static void gtk_prev_month_callback(GtkWidget *WXUNUSED(widget),
                                    wxGtkCalendarCtrl *cal)
{
    cal->GenerateEvent(wxEVT_CALENDAR_MONTH_CHANGED);
}

static void gtk_prev_year_callback(GtkWidget *WXUNUSED(widget),
                                    wxGtkCalendarCtrl *cal)
{
    cal->GenerateEvent(wxEVT_CALENDAR_YEAR_CHANGED);
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

bool wxGtkCalendarCtrl::EnableMonthChange(bool enable)
{
    if ( !wxCalendarCtrlBase::EnableMonthChange(enable) )
        return false;

    g_object_set (G_OBJECT (m_widget), "no-month-change", !enable, NULL);

    return true;
}


bool wxGtkCalendarCtrl::SetDate(const wxDateTime& date)
{
    g_signal_handlers_block_by_func(m_widget,
        (gpointer) gtk_day_selected_callback, this);

    m_selectedDate = date;
    int year = date.GetYear();
    int month = date.GetMonth();
    int day = date.GetDay();
    gtk_calendar_select_month(GTK_CALENDAR(m_widget), month, year);
    gtk_calendar_select_day(GTK_CALENDAR(m_widget), day);

    g_signal_handlers_unblock_by_func( m_widget,
        (gpointer) gtk_day_selected_callback, this);

    return true;
}

wxDateTime wxGtkCalendarCtrl::GetDate() const
{
    guint year, month, day;
    gtk_calendar_get_date(GTK_CALENDAR(m_widget), &year, &month, &day);
    return wxDateTime(day, (wxDateTime::Month) month, year);
}

void wxGtkCalendarCtrl::Mark(size_t day, bool mark)
{
    if (mark)
        gtk_calendar_mark_day(GTK_CALENDAR(m_widget), day);
    else
        gtk_calendar_unmark_day(GTK_CALENDAR(m_widget), day);
}

#endif // wxUSE_CALENDARCTRL
