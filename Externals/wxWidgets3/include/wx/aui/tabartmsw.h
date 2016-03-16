/////////////////////////////////////////////////////////////////////////////
// Name:        wx/aui/tabartmsw.h
// Purpose:     wxAuiMSWTabArt declaration
// Author:      Tobias Taschner
// Created:     2015-09-26
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_AUI_TABARTMSW_H_
#define _WX_AUI_TABARTMSW_H_

class WXDLLIMPEXP_AUI wxAuiMSWTabArt : public wxAuiGenericTabArt
{

public:

    wxAuiMSWTabArt();
    virtual ~wxAuiMSWTabArt();

    wxAuiTabArt* Clone() wxOVERRIDE;
    void SetSizingInfo(const wxSize& tabCtrlSize,
        size_t tabCount) wxOVERRIDE;

    void DrawBorder(
        wxDC& dc,
        wxWindow* wnd,
        const wxRect& rect) wxOVERRIDE;

    void DrawBackground(
        wxDC& dc,
        wxWindow* wnd,
        const wxRect& rect) wxOVERRIDE;

    void DrawTab(wxDC& dc,
        wxWindow* wnd,
        const wxAuiNotebookPage& pane,
        const wxRect& inRect,
        int closeButtonState,
        wxRect* outTabRect,
        wxRect* outButtonRect,
        int* xExtent) wxOVERRIDE;

    void DrawButton(
        wxDC& dc,
        wxWindow* wnd,
        const wxRect& inRect,
        int bitmapId,
        int buttonState,
        int orientation,
        wxRect* outRect) wxOVERRIDE;

    int GetIndentSize() wxOVERRIDE;

    int GetBorderWidth(
        wxWindow* wnd) wxOVERRIDE;

    int GetAdditionalBorderSpace(
        wxWindow* wnd) wxOVERRIDE;

    wxSize GetTabSize(
        wxDC& dc,
        wxWindow* wnd,
        const wxString& caption,
        const wxBitmap& bitmap,
        bool active,
        int closeButtonState,
        int* xExtent) wxOVERRIDE;

    int ShowDropDown(
        wxWindow* wnd,
        const wxAuiNotebookPageArray& items,
        int activeIdx) wxOVERRIDE;

    int GetBestTabCtrlSize(wxWindow* wnd,
        const wxAuiNotebookPageArray& pages,
        const wxSize& requiredBmpSize) wxOVERRIDE;

private:
    bool m_themed;
    wxSize m_closeBtnSize;
    wxSize m_tabSize;
    int m_maxTabHeight;

    void InitSizes(wxWindow* wnd, wxDC& dc);

    bool IsThemed() const;
};

#endif // _WX_AUI_TABARTMSW_H_
