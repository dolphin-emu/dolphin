/////////////////////////////////////////////////////////////////////////////
// Name:        wx/panel.h
// Purpose:     Base header for wxPanel
// Author:      Julian Smart
// Modified by:
// Created:
// RCS-ID:      $Id: panel.h 67253 2011-03-20 00:00:49Z VZ $
// Copyright:   (c) Julian Smart
//              (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PANEL_H_BASE_
#define _WX_PANEL_H_BASE_

// ----------------------------------------------------------------------------
// headers and forward declarations
// ----------------------------------------------------------------------------

#include "wx/window.h"
#include "wx/containr.h"

class WXDLLIMPEXP_FWD_CORE wxControlContainer;

extern WXDLLIMPEXP_DATA_CORE(const char) wxPanelNameStr[];

// ----------------------------------------------------------------------------
// wxPanel contains other controls and implements TAB traversal between them
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPanelBase : public wxWindow
{
public:
    wxPanelBase();

    // Derived classes should also provide this constructor:
    /*
    wxPanelBase(wxWindow *parent,
                wxWindowID winid = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTAB_TRAVERSAL | wxNO_BORDER,
                const wxString& name = wxPanelNameStr);
    */

    // Pseudo ctor
    bool Create(wxWindow *parent,
                wxWindowID winid = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTAB_TRAVERSAL | wxNO_BORDER,
                const wxString& name = wxPanelNameStr);


    // Use the given bitmap to tile the background of this panel. This bitmap
    // will show through any transparent children.
    //
    // Notice that you must not prevent the base class EVT_ERASE_BACKGROUND
    // handler from running (i.e. not to handle this event yourself) for this
    // to work.
    void SetBackgroundBitmap(const wxBitmap& bmp)
    {
        DoSetBackgroundBitmap(bmp);
    }


    // implementation from now on
    // --------------------------

    virtual void InitDialog();

    WX_DECLARE_CONTROL_CONTAINER();

protected:
    virtual void DoSetBackgroundBitmap(const wxBitmap& bmp) = 0;

private:
    wxDECLARE_EVENT_TABLE();

    wxDECLARE_NO_COPY_CLASS(wxPanelBase);
};

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/panel.h"
#elif defined(__WXMSW__)
    #include "wx/msw/panel.h"
#else
    #define wxHAS_GENERIC_PANEL
    #include "wx/generic/panelg.h"
#endif

#endif // _WX_PANELH_BASE_
