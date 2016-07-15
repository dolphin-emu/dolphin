///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/toplevel.h
// Purpose:     wxTopLevelWindowMac is the Mac implementation of wxTLW
// Author:      Stefan Csomor
// Modified by:
// Created:     20.09.01
// Copyright:   (c) 2001 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_TOPLEVEL_H_
#define _WX_MSW_TOPLEVEL_H_

// ----------------------------------------------------------------------------
// wxTopLevelWindowMac
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTopLevelWindowMac : public wxTopLevelWindowBase
{
public:
    // constructors and such
    wxTopLevelWindowMac() { Init(); }

    wxTopLevelWindowMac(wxWindow *parent,
                        wxWindowID id,
                        const wxString& title,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = wxDEFAULT_FRAME_STYLE,
                        const wxString& name = wxFrameNameStr)
    {
        Init();

        (void)Create(parent, id, title, pos, size, style, name);
    }

    virtual ~wxTopLevelWindowMac();

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    bool Create(wxWindow *parent, WXWindow nativeWindow);

    virtual bool Destroy() wxOVERRIDE;

    virtual wxPoint GetClientAreaOrigin() const wxOVERRIDE;

    // Attracts the users attention to this window if the application is
    // inactive (should be called when a background event occurs)
    virtual void RequestUserAttention(int flags = wxUSER_ATTENTION_INFO) wxOVERRIDE;

    // implement base class pure virtuals
    virtual void Maximize(bool maximize = true) wxOVERRIDE;
    virtual bool IsMaximized() const wxOVERRIDE;
    virtual void Iconize(bool iconize = true) wxOVERRIDE;
    virtual bool IsIconized() const wxOVERRIDE;
    virtual void Restore() wxOVERRIDE;

    virtual bool IsActive() wxOVERRIDE;

    virtual void ShowWithoutActivating() wxOVERRIDE;
    bool EnableFullScreenView(bool enable = true) wxOVERRIDE;
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) wxOVERRIDE;
    virtual bool IsFullScreen() const wxOVERRIDE;

    // implementation from now on
    // --------------------------

    virtual void SetTitle( const wxString& title) wxOVERRIDE;
    virtual wxString GetTitle() const wxOVERRIDE;

    // EnableCloseButton(false) used to disable the "Close"
    // button on the title bar
    virtual bool EnableCloseButton(bool enable = true) wxOVERRIDE;
    virtual bool EnableMaximizeButton(bool enable = true) wxOVERRIDE;
    virtual bool EnableMinimizeButton(bool enable = true) wxOVERRIDE;

    virtual void SetLabel(const wxString& label) wxOVERRIDE { SetTitle( label ); }
    virtual wxString GetLabel() const            wxOVERRIDE { return GetTitle(); }
    
    virtual void OSXSetModified(bool modified) wxOVERRIDE;
    virtual bool OSXIsModified() const wxOVERRIDE;

    virtual void SetRepresentedFilename(const wxString& filename) wxOVERRIDE;

    // do *not* call this to iconize the frame, this is a private function!
    void OSXSetIconizeState(bool iconic);

protected:
    // common part of all ctors
    void Init();

    // is the frame currently iconized?
    bool m_iconized;

    // should the frame be maximized when it will be shown? set by Maximize()
    // when it is called while the frame is hidden
    bool m_maximizeOnShow;

private :
    wxDECLARE_EVENT_TABLE();
};

#endif // _WX_MSW_TOPLEVEL_H_
