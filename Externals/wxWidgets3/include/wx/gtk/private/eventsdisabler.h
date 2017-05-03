///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/eventsdisabler.h
// Purpose:     Helper for temporarily disabling events.
// Author:      Vadim Zeitlin
// Created:     2016-02-06
// Copyright:   (c) 2016 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _GTK_PRIVATE_EVENTSDISABLER_H_
#define _GTK_PRIVATE_EVENTSDISABLER_H_

// ----------------------------------------------------------------------------
// wxGtkEventsDisabler: calls GTKDisableEvents() and GTKEnableEvents() in dtor.
// ----------------------------------------------------------------------------

// Template parameter T must be a wxGTK class providing the required methods,
// e.g. wxCheckBox, wxChoice, ...
template <typename T>
class wxGtkEventsDisabler
{
public:
    // Disable the events for the specified (non-NULL, having lifetime greater
    // than ours) window for the lifetime of this object.
    explicit wxGtkEventsDisabler(T* win) : m_win(win)
    {
        m_win->GTKDisableEvents();
    }

    ~wxGtkEventsDisabler()
    {
        m_win->GTKEnableEvents();
    }

private:
    T* const m_win;

    wxDECLARE_NO_COPY_TEMPLATE_CLASS(wxGtkEventsDisabler, T);
};

#endif // _GTK_PRIVATE_EVENTSDISABLER_H_
