/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/button.h
// Purpose:     wxButton class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_BUTTON_H_
#define _WX_OSX_BUTTON_H_

#include "wx/control.h"
#include "wx/gdicmn.h"

// Pushbutton
class WXDLLIMPEXP_CORE wxButton : public wxButtonBase
{
public:
    wxButton() {}
    wxButton(wxWindow *parent,
             wxWindowID id,
             const wxString& label = wxEmptyString,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxButtonNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

    virtual void SetLabel(const wxString& label);
    virtual wxWindow *SetDefault();
    virtual void Command(wxCommandEvent& event);

    // osx specific event handling common for all osx-ports

    virtual bool        OSXHandleClicked( double timestampsec );

#if wxOSX_USE_COCOA
    void OSXUpdateAfterLabelChange(const wxString& label);
#endif

protected:
    wxDECLARE_DYNAMIC_CLASS(wxButton);
};

#endif // _WX_OSX_BUTTON_H_
