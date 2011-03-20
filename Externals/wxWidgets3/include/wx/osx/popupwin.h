///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/popupwin.h
// Purpose:     wxPopupWindow class for wxMac
// Author:      Stefan Csomor
// Modified by:
// Created:
// RCS-ID:      $Id: popupwin.h 65680 2010-09-30 11:44:45Z VZ $
// Copyright:   (c) 2006 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MAC_POPUPWIN_H_
#define _WX_MAC_POPUPWIN_H_

// ----------------------------------------------------------------------------
// wxPopupWindow
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPopupWindow : public wxPopupWindowBase
{
public:
    wxPopupWindow() { }
    ~wxPopupWindow();

    wxPopupWindow(wxWindow *parent, int flags = wxBORDER_NONE)
        { (void)Create(parent, flags); }

    bool Create(wxWindow *parent, int flags = wxBORDER_NONE);

    virtual bool Show(bool show);

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxPopupWindow)
};

#endif // _WX_MAC_POPUPWIN_H_

