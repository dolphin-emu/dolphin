/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/popupwin.h
// Purpose:
// Author:      Robert Roebling
// Created:
// Copyright:   (c) 2001 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_POPUPWIN_H_
#define _WX_GTK_POPUPWIN_H_

//-----------------------------------------------------------------------------
// wxPopUpWindow
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPopupWindow: public wxPopupWindowBase
{
public:
    wxPopupWindow() { }
    virtual ~wxPopupWindow();

    wxPopupWindow(wxWindow *parent, int flags = wxBORDER_NONE)
        { (void)Create(parent, flags); }
    bool Create(wxWindow *parent, int flags = wxBORDER_NONE);

    virtual bool Show(bool show = true) wxOVERRIDE;

    virtual void SetFocus() wxOVERRIDE;

    // implementation
    // --------------

    // GTK time when connecting to button_press signal
    wxUint32  m_time;

protected:
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO) wxOVERRIDE;

    virtual void DoMoveWindow(int x, int y, int width, int height) wxOVERRIDE;

#ifdef __WXUNIVERSAL__
    wxDECLARE_EVENT_TABLE();
#endif
    wxDECLARE_DYNAMIC_CLASS(wxPopupWindow);
};

#endif // _WX_GTK_POPUPWIN_H_
