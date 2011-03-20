///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/nonownedwnd.h
// Purpose:     declares wxNonOwnedWindow class
// Author:      Stefan Csomor
// Modified by:
// Created:     2008-03-24
// RCS-ID:      $Id: nonownedwnd.h 67232 2011-03-18 15:10:15Z DS $
// Copyright:   (c) 2008 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MAC_NONOWNEDWND_H_
#define _WX_MAC_NONOWNEDWND_H_

#include "wx/window.h"

#if wxUSE_SYSTEM_OPTIONS
    #define wxMAC_WINDOW_PLAIN_TRANSITION wxT("mac.window-plain-transition")
#endif

//-----------------------------------------------------------------------------
// wxNonOwnedWindow
//-----------------------------------------------------------------------------

// This class represents "non-owned" window. A window is owned by another
// window if it has a parent and is positioned within the parent. For example,
// wxFrame is non-owned, because even though it can have a parent, it's
// location is independent of it.  This class is for internal use only, it's
// the base class for wxTopLevelWindow and wxPopupWindow.

class wxNonOwnedWindowImpl;

class WXDLLIMPEXP_CORE wxNonOwnedWindow : public wxWindow
{
public:
    // constructors and such
    wxNonOwnedWindow() { Init(); }

    wxNonOwnedWindow(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = 0,
                        const wxString& name = wxPanelNameStr)
    {
        Init();

        (void)Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr);

    bool Create(wxWindow *parent, WXWindow nativeWindow);

    virtual ~wxNonOwnedWindow();

    virtual void SubclassWin(WXWindow nativeWindow);
    virtual void UnsubclassWin();

    virtual wxPoint GetClientAreaOrigin() const;

    // implement base class pure virtuals

    virtual bool SetTransparent(wxByte alpha);
    virtual bool CanSetTransparent();

    virtual bool SetBackgroundStyle(wxBackgroundStyle style);

    virtual void Update();

    WXWindow GetWXWindow() const ;
    static wxNonOwnedWindow* GetFromWXWindow( WXWindow win );

    // implementation from now on
    // --------------------------

    virtual bool DoSetShape(const wxRegion& region);
    const wxRegion& GetShape() const { return m_shape; }

    // activation hooks only necessary for MDI Implementation
    static void MacDelayedDeactivation(long timestamp);
    virtual void MacActivate( long timestamp , bool inIsActivating ) ;

    virtual void SetWindowStyleFlag(long flags);

    virtual void Raise();
    virtual void Lower();
    virtual bool Show( bool show = true );

    virtual void SetExtraStyle(long exStyle) ;

    virtual bool SetBackgroundColour( const wxColour &colour );

    wxNonOwnedWindowImpl* GetNonOwnedPeer() const { return m_nowpeer; }

#if wxOSX_USE_COCOA_OR_IPHONE
    // override the base class method to return an NSWindow instead of NSView
    virtual void *OSXGetViewOrWindow() const { return GetWXWindow(); }
#endif // Cocoa

    // osx specific event handling common for all osx-ports

    virtual void HandleActivated( double timestampsec, bool didActivate );
    virtual void HandleResized( double timestampsec );
    virtual void HandleMoved( double timestampsec );
    virtual void HandleResizing( double timestampsec, wxRect* rect );

    virtual bool Destroy();

protected:
    // common part of all ctors
    void Init();

    virtual void DoGetPosition( int *x, int *y ) const;
    virtual void DoGetSize( int *width, int *height ) const;
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual void DoGetClientSize(int *width, int *height) const;

    virtual bool OSXShowWithEffect(bool show,
                                   wxShowEffect effect,
                                   unsigned timeout);

    virtual void WillBeDestroyed();

    wxNonOwnedWindowImpl* m_nowpeer ;

//    wxWindowMac* m_macFocus ;

    static wxNonOwnedWindow *s_macDeactivateWindow;

private :
    wxRegion m_shape;
};

// list of all frames and modeless dialogs
extern WXDLLIMPEXP_DATA_CORE(wxWindowList) wxModelessWindows;


#endif // _WX_MAC_NONOWNEDWND_H_
