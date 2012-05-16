///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/timectrl.h
// Purpose:     Declaration of wxOSX-specific wxTimePickerCtrl class.
// Author:      Vadim Zeitlin
// Created:     2011-12-18
// RCS-ID:      $Id: timectrl.h 70071 2011-12-20 21:27:14Z VZ $
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_TIMECTRL_H_
#define _WX_OSX_TIMECTRL_H_

// ----------------------------------------------------------------------------
// wxTimePickerCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxTimePickerCtrl : public wxTimePickerCtrlBase
{
public:
    // Constructors.
    wxTimePickerCtrl() { }

    wxTimePickerCtrl(wxWindow *parent,
                     wxWindowID id,
                     const wxDateTime& dt = wxDefaultDateTime,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxTP_DEFAULT,
                     const wxValidator& validator = wxDefaultValidator,
                     const wxString& name = wxTimePickerCtrlNameStr)
    {
        Create(parent, id, dt, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxDateTime& dt = wxDefaultDateTime,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTP_DEFAULT,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTimePickerCtrlNameStr);

    virtual void OSXGenerateEvent(const wxDateTime& dt);

private:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxTimePickerCtrl);
};

#endif // _WX_OSX_TIMECTRL_H_
