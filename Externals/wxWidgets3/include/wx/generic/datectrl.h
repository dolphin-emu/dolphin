/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/datectrl.h
// Purpose:     generic wxDatePickerCtrl implementation
// Author:      Andreas Pflug
// Modified by:
// Created:     2005-01-19
// Copyright:   (c) 2005 Andreas Pflug <pgadmin@pse-consulting.de>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_DATECTRL_H_
#define _WX_GENERIC_DATECTRL_H_

#include "wx/compositewin.h"

class WXDLLIMPEXP_FWD_CORE wxComboCtrl;

class WXDLLIMPEXP_FWD_ADV wxCalendarCtrl;
class WXDLLIMPEXP_FWD_ADV wxCalendarComboPopup;

class WXDLLIMPEXP_ADV wxDatePickerCtrlGeneric
    : public wxCompositeWindow<wxDatePickerCtrlBase>
{
public:
    // creating the control
    wxDatePickerCtrlGeneric() { Init(); }
    virtual ~wxDatePickerCtrlGeneric();
    wxDatePickerCtrlGeneric(wxWindow *parent,
                            wxWindowID id,
                            const wxDateTime& date = wxDefaultDateTime,
                            const wxPoint& pos = wxDefaultPosition,
                            const wxSize& size = wxDefaultSize,
                            long style = wxDP_DEFAULT | wxDP_SHOWCENTURY,
                            const wxValidator& validator = wxDefaultValidator,
                            const wxString& name = wxDatePickerCtrlNameStr)
    {
        Init();

        (void)Create(parent, id, date, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxDateTime& date = wxDefaultDateTime,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDP_DEFAULT | wxDP_SHOWCENTURY,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxDatePickerCtrlNameStr);

    // wxDatePickerCtrl methods
    void SetValue(const wxDateTime& date) wxOVERRIDE;
    wxDateTime GetValue() const wxOVERRIDE;

    bool GetRange(wxDateTime *dt1, wxDateTime *dt2) const wxOVERRIDE;
    void SetRange(const wxDateTime &dt1, const wxDateTime &dt2) wxOVERRIDE;

    bool SetDateRange(const wxDateTime& lowerdate = wxDefaultDateTime,
                      const wxDateTime& upperdate = wxDefaultDateTime);

    // extra methods available only in this (generic) implementation
    wxCalendarCtrl *GetCalendar() const;


    // implementation only from now on
    // -------------------------------

    // overridden base class methods
    virtual bool Destroy() wxOVERRIDE;

protected:
    virtual wxSize DoGetBestSize() const wxOVERRIDE;

private:
    void Init();

    // return the list of the windows composing this one
    virtual wxWindowList GetCompositeWindowParts() const wxOVERRIDE;

    void OnText(wxCommandEvent &event);
    void OnSize(wxSizeEvent& event);
    void OnFocus(wxFocusEvent& event);

#ifdef __WXOSX_COCOA__
    virtual void OSXGenerateEvent(const wxDateTime& WXUNUSED(dt)) wxOVERRIDE { }
#endif

    wxComboCtrl* m_combo;
    wxCalendarComboPopup* m_popup;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxDatePickerCtrlGeneric);
};

#endif // _WX_GENERIC_DATECTRL_H_

