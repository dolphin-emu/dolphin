/////////////////////////////////////////////////////////////////////////////
// Name:        wx/aui/barartmsw.h
// Purpose:     Interface of wxAuiMSWToolBarArt
// Author:      Tobias Taschner
// Created:     2015-09-22
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_AUI_BARART_MSW_H_
#define _WX_AUI_BARART_MSW_H_

class WXDLLIMPEXP_AUI wxAuiMSWToolBarArt : public wxAuiGenericToolBarArt
{
public:
    wxAuiMSWToolBarArt();

    virtual wxAuiToolBarArt* Clone() wxOVERRIDE;

    virtual void DrawBackground(
        wxDC& dc,
        wxWindow* wnd,
        const wxRect& rect) wxOVERRIDE;

    virtual void DrawLabel(
        wxDC& dc,
        wxWindow* wnd,
        const wxAuiToolBarItem& item,
        const wxRect& rect) wxOVERRIDE;

    virtual void DrawButton(
        wxDC& dc,
        wxWindow* wnd,
        const wxAuiToolBarItem& item,
        const wxRect& rect) wxOVERRIDE;

    virtual void DrawDropDownButton(
        wxDC& dc,
        wxWindow* wnd,
        const wxAuiToolBarItem& item,
        const wxRect& rect) wxOVERRIDE;

    virtual void DrawControlLabel(
        wxDC& dc,
        wxWindow* wnd,
        const wxAuiToolBarItem& item,
        const wxRect& rect) wxOVERRIDE;

    virtual void DrawSeparator(
        wxDC& dc,
        wxWindow* wnd,
        const wxRect& rect) wxOVERRIDE;

    virtual void DrawGripper(
        wxDC& dc,
        wxWindow* wnd,
        const wxRect& rect) wxOVERRIDE;

    virtual void DrawOverflowButton(
        wxDC& dc,
        wxWindow* wnd,
        const wxRect& rect,
        int state) wxOVERRIDE;

    virtual wxSize GetLabelSize(
        wxDC& dc,
        wxWindow* wnd,
        const wxAuiToolBarItem& item) wxOVERRIDE;

    virtual wxSize GetToolSize(
        wxDC& dc,
        wxWindow* wnd,
        const wxAuiToolBarItem& item) wxOVERRIDE;

    virtual int GetElementSize(int element) wxOVERRIDE;
    virtual void SetElementSize(int elementId, int size) wxOVERRIDE;

    virtual int ShowDropDown(wxWindow* wnd,
        const wxAuiToolBarItemArray& items) wxOVERRIDE;

private:
    bool m_themed;
    wxSize m_buttonSize;
};

#endif // _WX_AUI_BARART_MSW_H_
