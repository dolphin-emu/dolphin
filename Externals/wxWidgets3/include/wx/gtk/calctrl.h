/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/calctrl.h
// Purpose:     wxGtkCalendarCtrl control
// Author:      Marcin Wojdyr
// RCS-ID:      $Id: calctrl.h 58757 2009-02-08 11:45:59Z VZ $
// Copyright:   (C) 2008 Marcin Wojdyr
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef GTK_CALCTRL_H__
#define GTK_CALCTRL_H__

class WXDLLIMPEXP_ADV wxGtkCalendarCtrl : public wxCalendarCtrlBase
{
public:
    wxGtkCalendarCtrl() {}
    wxGtkCalendarCtrl(wxWindow *parent,
                          wxWindowID id,
                          const wxDateTime& date = wxDefaultDateTime,
                          const wxPoint& pos = wxDefaultPosition,
                          const wxSize& size = wxDefaultSize,
                          long style = wxCAL_SHOW_HOLIDAYS,
                          const wxString& name = wxCalendarNameStr)
    {
        Create(parent, id, date, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxDateTime& date = wxDefaultDateTime,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxCAL_SHOW_HOLIDAYS,
                const wxString& name = wxCalendarNameStr);

    virtual ~wxGtkCalendarCtrl() {}

    virtual bool SetDate(const wxDateTime& date);
    virtual wxDateTime GetDate() const;

    virtual bool EnableMonthChange(bool enable = true);

    virtual void Mark(size_t day, bool mark);

    // implementation
    // --------------
    wxDateTime m_selectedDate;

private:
    DECLARE_DYNAMIC_CLASS(wxGtkCalendarCtrl)
    wxDECLARE_NO_COPY_CLASS(wxGtkCalendarCtrl);
};

#endif // GTK_CALCTRL_H__
