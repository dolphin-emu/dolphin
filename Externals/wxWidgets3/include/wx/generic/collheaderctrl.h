/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/collheaderctrl.h
// Purpose:     wxGenericCollapsibleHeaderCtrl
// Author:      Tobias Taschner
// Created:     2015-09-19
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_COLLAPSIBLEHEADER_CTRL_H_
#define _WX_GENERIC_COLLAPSIBLEHEADER_CTRL_H_

class WXDLLIMPEXP_CORE wxGenericCollapsibleHeaderCtrl
    : public wxCollapsibleHeaderCtrlBase
{
public:
    wxGenericCollapsibleHeaderCtrl() { Init(); }

    wxGenericCollapsibleHeaderCtrl(wxWindow *parent,
        wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxBORDER_NONE,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxCollapsibleHeaderCtrlNameStr)
    {
        Init();

        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
        wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxBORDER_NONE,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxCollapsibleHeaderCtrlNameStr);

    virtual void SetCollapsed(bool collapsed = true) wxOVERRIDE;

    virtual bool IsCollapsed() const wxOVERRIDE
        { return m_collapsed; }

protected:

    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;

private:
    bool m_collapsed;
    bool m_inWindow;
    bool m_mouseDown;

    void Init();

    void OnPaint(wxPaintEvent& event);

    // Handle set/kill focus events (invalidate for painting focus rect)
    void OnFocus(wxFocusEvent& event);

    // Handle click
    void OnLeftUp(wxMouseEvent& event);

    // Handle pressed state
    void OnLeftDown(wxMouseEvent& event);

    // Handle current state
    void OnEnterWindow(wxMouseEvent& event);

    void OnLeaveWindow(wxMouseEvent& event);

    // Toggle on space
    void OnChar(wxKeyEvent& event);

    void DoSetCollapsed(bool collapsed);

    wxDECLARE_NO_COPY_CLASS(wxGenericCollapsibleHeaderCtrl);
};


#endif // _WX_GENERIC_COLLAPSIBLEHEADER_CTRL_H_
