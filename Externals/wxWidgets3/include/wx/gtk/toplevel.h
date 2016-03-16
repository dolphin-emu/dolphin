/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/toplevel.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_TOPLEVEL_H_
#define _WX_GTK_TOPLEVEL_H_

//-----------------------------------------------------------------------------
// wxTopLevelWindowGTK
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTopLevelWindowGTK : public wxTopLevelWindowBase
{
    typedef wxTopLevelWindowBase base_type;
public:
    // construction
    wxTopLevelWindowGTK() { Init(); }
    wxTopLevelWindowGTK(wxWindow *parent,
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

    virtual ~wxTopLevelWindowGTK();

    // implement base class pure virtuals
    virtual void Maximize(bool maximize = true) wxOVERRIDE;
    virtual bool IsMaximized() const wxOVERRIDE;
    virtual void Iconize(bool iconize = true) wxOVERRIDE;
    virtual bool IsIconized() const wxOVERRIDE;
    virtual void SetIcons(const wxIconBundle& icons) wxOVERRIDE;
    virtual void Restore() wxOVERRIDE;

    virtual bool EnableCloseButton(bool enable = true) wxOVERRIDE;

    virtual void ShowWithoutActivating() wxOVERRIDE;
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) wxOVERRIDE;
    virtual bool IsFullScreen() const wxOVERRIDE { return m_fsIsShowing; }

    virtual void RequestUserAttention(int flags = wxUSER_ATTENTION_INFO) wxOVERRIDE;

    virtual void SetWindowStyleFlag( long style ) wxOVERRIDE;

    virtual bool Show(bool show = true) wxOVERRIDE;

    virtual void Raise() wxOVERRIDE;

    virtual bool IsActive() wxOVERRIDE;

    virtual void SetTitle( const wxString &title ) wxOVERRIDE;
    virtual wxString GetTitle() const wxOVERRIDE { return m_title; }

    virtual void SetLabel(const wxString& label) wxOVERRIDE { SetTitle( label ); }
    virtual wxString GetLabel() const wxOVERRIDE            { return GetTitle(); }


    virtual bool SetTransparent(wxByte alpha) wxOVERRIDE;
    virtual bool CanSetTransparent() wxOVERRIDE;

    // Experimental, to allow help windows to be
    // viewable from within modal dialogs
    virtual void AddGrab();
    virtual void RemoveGrab();
    virtual bool IsGrabbed() const { return m_grabbed; }


    virtual void Refresh( bool eraseBackground = true,
                          const wxRect *rect = (const wxRect *) NULL ) wxOVERRIDE;

    // implementation from now on
    // --------------------------

    // GTK callbacks
    virtual void GTKHandleRealized() wxOVERRIDE;

    void GTKConfigureEvent(int x, int y);

    // do *not* call this to iconize the frame, this is a private function!
    void SetIconizeState(bool iconic);

    GtkWidget    *m_mainWidget;

    bool          m_fsIsShowing;         /* full screen */
    int           m_fsSaveGdkFunc, m_fsSaveGdkDecor;
    wxRect        m_fsSaveFrame;

    // m_windowStyle translated to GDK's terms
    int           m_gdkFunc,
                  m_gdkDecor;

    // size of WM decorations
    struct DecorSize
    {
        int left, right, top, bottom;
    };
    DecorSize m_decorSize;

    // private gtk_timeout_add result for mimicing wxUSER_ATTENTION_INFO and
    // wxUSER_ATTENTION_ERROR difference, -2 for no hint, -1 for ERROR hint, rest for GtkTimeout handle.
    int m_urgency_hint;
    // timer for detecting WM with broken _NET_REQUEST_FRAME_EXTENTS handling
    unsigned m_netFrameExtentsTimerId;

    // return the size of the window without WM decorations
    void GTKDoGetSize(int *width, int *height) const;

    void GTKUpdateDecorSize(const DecorSize& decorSize);

protected:
    // give hints to the Window Manager for how the size
    // of the TLW can be changed by dragging
    virtual void DoSetSizeHints( int minW, int minH,
                                 int maxW, int maxH,
                                 int incW, int incH) wxOVERRIDE;
    // move the window to the specified location and resize it
    virtual void DoMoveWindow(int x, int y, int width, int height) wxOVERRIDE;

    // take into account WM decorations here
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO) wxOVERRIDE;

    virtual void DoSetClientSize(int width, int height) wxOVERRIDE;
    virtual void DoGetClientSize(int *width, int *height) const wxOVERRIDE;

    // string shown in the title bar
    wxString m_title;

    bool m_deferShow;

private:
    void Init();
    DecorSize& GetCachedDecorSize();

    // size hint increments
    int m_incWidth, m_incHeight;

    // is the frame currently iconized?
    bool m_isIconized;

    // is the frame currently grabbed explicitly by the application?
    bool m_grabbed;

    bool m_updateDecorSize;
    bool m_deferShowAllowed;
};

#endif // _WX_GTK_TOPLEVEL_H_
