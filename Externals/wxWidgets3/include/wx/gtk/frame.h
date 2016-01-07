/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/frame.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_FRAME_H_
#define _WX_GTK_FRAME_H_

//-----------------------------------------------------------------------------
// wxFrame
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFrame : public wxFrameBase
{
public:
    // construction
    wxFrame() { Init(); }
    wxFrame(wxWindow *parent,
               wxWindowID id,
               const wxString& title,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxDEFAULT_FRAME_STYLE,
               const wxString& name = wxFrameNameStr)
    {
        Init();

        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

#if wxUSE_STATUSBAR
    void SetStatusBar(wxStatusBar *statbar) wxOVERRIDE;
#endif // wxUSE_STATUSBAR

#if wxUSE_TOOLBAR
    void SetToolBar(wxToolBar *toolbar) wxOVERRIDE;
#endif // wxUSE_TOOLBAR

    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) wxOVERRIDE;
    wxPoint GetClientAreaOrigin() const wxOVERRIDE { return wxPoint(0, 0); }

    // implementation from now on
    // --------------------------

    virtual bool SendIdleEvents(wxIdleEvent& event) wxOVERRIDE;

protected:
    // override wxWindow methods to take into account tool/menu/statusbars
    virtual void DoGetClientSize( int *width, int *height ) const wxOVERRIDE;

#if wxUSE_MENUS_NATIVE
    virtual void DetachMenuBar() wxOVERRIDE;
    virtual void AttachMenuBar(wxMenuBar *menubar) wxOVERRIDE;
#endif // wxUSE_MENUS_NATIVE

private:
    void Init();

    long m_fsSaveFlag;

    wxDECLARE_DYNAMIC_CLASS(wxFrame);
};

#endif // _WX_GTK_FRAME_H_
