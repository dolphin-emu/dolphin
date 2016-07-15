/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/osx/webkit.h
// Purpose:     wxWebViewWebKit - embeddable web kit control,
//                             OS X implementation of web view component
// Author:      Jethro Grassie / Kevin Ollivier / Marianne Gagnon
// Modified by:
// Created:     2004-4-16
// Copyright:   (c) Jethro Grassie / Kevin Ollivier / Marianne Gagnon
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WEBKIT_H
#define _WX_WEBKIT_H

#include "wx/defs.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT && defined(__WXOSX__)

#include "wx/control.h"
#include "wx/webview.h"

#include "wx/osx/core/objcid.h"

// ----------------------------------------------------------------------------
// Web Kit Control
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_WEBVIEW wxWebViewWebKit : public wxWebView
{
public:
    wxDECLARE_DYNAMIC_CLASS(wxWebViewWebKit);

    wxWebViewWebKit() {}
    wxWebViewWebKit(wxWindow *parent,
                    wxWindowID winID = wxID_ANY,
                    const wxString& strURL = wxWebViewDefaultURLStr,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize, long style = 0,
                    const wxString& name = wxWebViewNameStr)
    {
        Create(parent, winID, strURL, pos, size, style, name);
    }
    bool Create(wxWindow *parent,
                wxWindowID winID = wxID_ANY,
                const wxString& strURL = wxWebViewDefaultURLStr,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = 0,
                const wxString& name = wxWebViewNameStr) wxOVERRIDE;
    virtual ~wxWebViewWebKit();

    virtual bool CanGoBack() const wxOVERRIDE;
    virtual bool CanGoForward() const wxOVERRIDE;
    virtual void GoBack() wxOVERRIDE;
    virtual void GoForward() wxOVERRIDE;
    virtual void Reload(wxWebViewReloadFlags flags = wxWEBVIEW_RELOAD_DEFAULT) wxOVERRIDE;
    virtual void Stop() wxOVERRIDE;
    virtual wxString GetPageSource() const wxOVERRIDE;
    virtual wxString GetPageText() const wxOVERRIDE;

    virtual void Print() wxOVERRIDE;

    virtual void LoadURL(const wxString& url) wxOVERRIDE;
    virtual wxString GetCurrentURL() const wxOVERRIDE;
    virtual wxString GetCurrentTitle() const wxOVERRIDE;
    virtual wxWebViewZoom GetZoom() const wxOVERRIDE;
    virtual void SetZoom(wxWebViewZoom zoom) wxOVERRIDE;

    virtual void SetZoomType(wxWebViewZoomType zoomType) wxOVERRIDE;
    virtual wxWebViewZoomType GetZoomType() const wxOVERRIDE;
    virtual bool CanSetZoomType(wxWebViewZoomType type) const wxOVERRIDE;

    virtual bool IsBusy() const wxOVERRIDE { return m_busy; }

    //History functions
    virtual void ClearHistory() wxOVERRIDE;
    virtual void EnableHistory(bool enable = true) wxOVERRIDE;
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory() wxOVERRIDE;
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory() wxOVERRIDE;
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item) wxOVERRIDE;

    //Undo / redo functionality
    virtual bool CanUndo() const wxOVERRIDE;
    virtual bool CanRedo() const wxOVERRIDE;
    virtual void Undo() wxOVERRIDE;
    virtual void Redo() wxOVERRIDE;

    //Find function
    virtual long Find(const wxString& text, int flags = wxWEBVIEW_FIND_DEFAULT) wxOVERRIDE
    { 
        wxUnusedVar(text);
        wxUnusedVar(flags);
        return wxNOT_FOUND; 
    }

    //Clipboard functions
    virtual bool CanCut() const wxOVERRIDE { return true; }
    virtual bool CanCopy() const wxOVERRIDE { return true; }
    virtual bool CanPaste() const wxOVERRIDE { return true; }
    virtual void Cut() wxOVERRIDE;
    virtual void Copy() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;

    //Editing functions
    virtual void SetEditable(bool enable = true) wxOVERRIDE;
    virtual bool IsEditable() const wxOVERRIDE;

    //Selection
    virtual void DeleteSelection() wxOVERRIDE;
    virtual bool HasSelection() const wxOVERRIDE;
    virtual void SelectAll() wxOVERRIDE;
    virtual wxString GetSelectedText() const wxOVERRIDE;
    virtual wxString GetSelectedSource() const wxOVERRIDE;
    virtual void ClearSelection() wxOVERRIDE;

    void RunScript(const wxString& javascript) wxOVERRIDE;

    //Virtual Filesystem Support
    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler) wxOVERRIDE;

    virtual void* GetNativeBackend() const wxOVERRIDE { return m_webView; }

    // ---- methods not from the parent (common) interface
    bool  CanGetPageSource() const;

    void  SetScrollPos(int pos);
    int   GetScrollPos();

    bool  CanIncreaseTextSize() const;
    void  IncreaseTextSize();
    bool  CanDecreaseTextSize() const;
    void  DecreaseTextSize();

    float GetWebkitZoom() const;
    void  SetWebkitZoom(float zoom);

    // don't hide base class virtuals
    virtual void SetScrollPos( int orient, int pos, bool refresh = true ) wxOVERRIDE
        { return wxControl::SetScrollPos(orient, pos, refresh); }
    virtual int GetScrollPos( int orient ) const wxOVERRIDE
        { return wxControl::GetScrollPos(orient); }

    //we need to resize the webview when the control size changes
    void OnSize(wxSizeEvent &event);
    void OnMove(wxMoveEvent &event);
    void OnMouseEvents(wxMouseEvent &event);

    bool m_busy;
    bool m_nextNavigationIsNewWindow;

protected:
    virtual void DoSetPage(const wxString& html, const wxString& baseUrl) wxOVERRIDE;

    wxDECLARE_EVENT_TABLE();
    void MacVisibilityChanged() wxOVERRIDE;

private:
    wxWindow *m_parent;
    wxWindowID m_windowID;
    wxString m_pageTitle;

    OSXWebViewPtr m_webView;

    // we may use this later to setup our own mouse events,
    // so leave it in for now.
    void* m_webKitCtrlEventHandler;
    //It should be WebView*, but WebView is an Objective-C class
    //TODO: look into using DECLARE_WXCOCOA_OBJC_CLASS rather than this.
};

class WXDLLIMPEXP_WEBVIEW wxWebViewFactoryWebKit : public wxWebViewFactory
{
public:
    virtual wxWebView* Create() { return new wxWebViewWebKit; }
    virtual wxWebView* Create(wxWindow* parent,
                              wxWindowID id,
                              const wxString& url = wxWebViewDefaultURLStr,
                              const wxPoint& pos = wxDefaultPosition,
                              const wxSize& size = wxDefaultSize,
                              long style = 0,
                              const wxString& name = wxWebViewNameStr)
    { return new wxWebViewWebKit(parent, id, url, pos, size, style, name); }
};

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT

#endif // _WX_WEBKIT_H_
