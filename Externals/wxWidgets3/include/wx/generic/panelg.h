/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/panelg.h
// Purpose:     wxPanel: a container for child controls
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: panelg.h 67253 2011-03-20 00:00:49Z VZ $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_PANELG_H_
#define _WX_GENERIC_PANELG_H_

#include "wx/bitmap.h"

class WXDLLIMPEXP_CORE wxPanel : public wxPanelBase
{
public:
    wxPanel() { }

    // Constructor
    wxPanel(wxWindow *parent,
            wxWindowID winid = wxID_ANY,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxTAB_TRAVERSAL | wxNO_BORDER,
            const wxString& name = wxPanelNameStr)
    {
        Create(parent, winid, pos, size, style, name);
    }

#ifdef WXWIN_COMPATIBILITY_2_8
    wxDEPRECATED_CONSTRUCTOR(
    wxPanel(wxWindow *parent,
            int x, int y, int width, int height,
            long style = wxTAB_TRAVERSAL | wxNO_BORDER,
            const wxString& name = wxPanelNameStr)
    {
        Create(parent, wxID_ANY, wxPoint(x, y), wxSize(width, height), style, name);
    }
    )
#endif // WXWIN_COMPATIBILITY_2_8

protected:
    virtual void DoSetBackgroundBitmap(const wxBitmap& bmp);

private:
    // Event handler for erasing the background which is only used when we have
    // a valid background bitmap.
    void OnEraseBackground(wxEraseEvent& event);


    // The bitmap used for painting the background if valid.
    wxBitmap m_bitmapBg;

    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxPanel);
};

#endif // _WX_GENERIC_PANELG_H_
