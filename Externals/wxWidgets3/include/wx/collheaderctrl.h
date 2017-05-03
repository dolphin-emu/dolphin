/////////////////////////////////////////////////////////////////////////////
// Name:        wx/collheaderctrl.h
// Purpose:     wxCollapsibleHeaderCtrl
// Author:      Tobias Taschner
// Created:     2015-09-19
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLLAPSIBLEHEADER_CTRL_H_
#define _WX_COLLAPSIBLEHEADER_CTRL_H_

#include "wx/defs.h"

#if wxUSE_COLLPANE

#include "wx/control.h"

// class name
extern WXDLLIMPEXP_DATA_CORE(const char) wxCollapsibleHeaderCtrlNameStr[];

//
// wxGenericCollapsibleHeaderCtrl
//

class WXDLLIMPEXP_CORE wxCollapsibleHeaderCtrlBase : public wxControl
{
public:
    wxCollapsibleHeaderCtrlBase() { }

    wxCollapsibleHeaderCtrlBase(wxWindow *parent,
        wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxBORDER_NONE,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxCollapsibleHeaderCtrlNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
        wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxBORDER_NONE,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxCollapsibleHeaderCtrlNameStr)
    {
        if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
            return false;

        SetLabel(label);

        return true;
    }

    virtual void SetCollapsed(bool collapsed = true) = 0;

    virtual bool IsCollapsed() const = 0;

private:

    wxDECLARE_NO_COPY_CLASS(wxCollapsibleHeaderCtrlBase);
};

wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_COLLAPSIBLEHEADER_CHANGED, wxCommandEvent);

#define wxCollapsibleHeaderChangedHandler(func) \
    wxEVENT_HANDLER_CAST(wxCommandEventFunction, func)

#define EVT_COLLAPSIBLEHEADER_CHANGED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_COLLAPSIBLEHEADER_CHANGED, id, wxCollapsibleHeaderChangedHandler(fn))

// Currently there is only the native implementation, use it for all ports.

#include "wx/generic/collheaderctrl.h"

class WXDLLIMPEXP_CORE wxCollapsibleHeaderCtrl
    : public wxGenericCollapsibleHeaderCtrl
{
public:
    wxCollapsibleHeaderCtrl() { }

    wxCollapsibleHeaderCtrl(wxWindow *parent,
        wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxBORDER_NONE,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxCollapsibleHeaderCtrlNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

private:

    wxDECLARE_NO_COPY_CLASS(wxCollapsibleHeaderCtrl);
};

#endif // wxUSE_COLLPANE

#endif // _WX_COLLAPSIBLEHEADER_CTRL_H_
