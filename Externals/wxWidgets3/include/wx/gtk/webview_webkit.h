/////////////////////////////////////////////////////////////////////////////
// Name:        include/gtk/wx/webview.h
// Purpose:     GTK webkit backend for web view component
// Author:      Robert Roebling, Marianne Gagnon
// Copyright:   (c) 2010 Marianne Gagnon, 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_WEBKITCTRL_H_
#define _WX_GTK_WEBKITCTRL_H_

#include "wx/defs.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT && defined(__WXGTK__)

#include "wx/sharedptr.h"
#include "wx/webview.h"

typedef struct _WebKitWebView WebKitWebView;

//-----------------------------------------------------------------------------
// wxWebViewWebKit
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_WEBVIEW wxWebViewWebKit : public wxWebView
{
public:
    wxWebViewWebKit();

    wxWebViewWebKit(wxWindow *parent,
           wxWindowID id = wxID_ANY,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxString& name = wxWebViewNameStr)
    {
        Create(parent, id, url, pos, size, style, name);
    }

    virtual bool Create(wxWindow *parent,
           wxWindowID id = wxID_ANY,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxString& name = wxWebViewNameStr) wxOVERRIDE;

    virtual ~wxWebViewWebKit();

    virtual bool Enable( bool enable = true ) wxOVERRIDE;

    // implementation
    // --------------

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    virtual void Stop() wxOVERRIDE;
    virtual void LoadURL(const wxString& url) wxOVERRIDE;
    virtual void GoBack() wxOVERRIDE;
    virtual void GoForward() wxOVERRIDE;
    virtual void Reload(wxWebViewReloadFlags flags = wxWEBVIEW_RELOAD_DEFAULT) wxOVERRIDE;
    virtual bool CanGoBack() const wxOVERRIDE;
    virtual bool CanGoForward() const wxOVERRIDE;
    virtual void ClearHistory() wxOVERRIDE;
    virtual void EnableContextMenu(bool enable = true) wxOVERRIDE;
    virtual void EnableHistory(bool enable = true) wxOVERRIDE;
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory() wxOVERRIDE;
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory() wxOVERRIDE;
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item) wxOVERRIDE;
    virtual wxString GetCurrentURL() const wxOVERRIDE;
    virtual wxString GetCurrentTitle() const wxOVERRIDE;
    virtual wxString GetPageSource() const wxOVERRIDE;
    virtual wxString GetPageText() const wxOVERRIDE;
    virtual void Print() wxOVERRIDE;
    virtual bool IsBusy() const wxOVERRIDE;

    void SetZoomType(wxWebViewZoomType) wxOVERRIDE;
    wxWebViewZoomType GetZoomType() const wxOVERRIDE;
    bool CanSetZoomType(wxWebViewZoomType) const wxOVERRIDE;
    virtual wxWebViewZoom GetZoom() const wxOVERRIDE;
    virtual void SetZoom(wxWebViewZoom) wxOVERRIDE;

    //Clipboard functions
    virtual bool CanCut() const wxOVERRIDE;
    virtual bool CanCopy() const wxOVERRIDE;
    virtual bool CanPaste() const wxOVERRIDE;
    virtual void Cut() wxOVERRIDE;
    virtual void Copy() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;

    //Undo / redo functionality
    virtual bool CanUndo() const wxOVERRIDE;
    virtual bool CanRedo() const wxOVERRIDE;
    virtual void Undo() wxOVERRIDE;
    virtual void Redo() wxOVERRIDE;

    //Find function
    virtual long Find(const wxString& text, int flags = wxWEBVIEW_FIND_DEFAULT) wxOVERRIDE;

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

    virtual void RunScript(const wxString& javascript) wxOVERRIDE;

    //Virtual Filesystem Support
    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler) wxOVERRIDE;
    virtual wxVector<wxSharedPtr<wxWebViewHandler> > GetHandlers() { return m_handlerList; }

    virtual void* GetNativeBackend() const wxOVERRIDE { return m_web_view; }

    /** TODO: check if this can be made private
     * The native control has a getter to check for busy state, but except in
     * very recent versions of webkit this getter doesn't say everything we need
     * (namely it seems to stay indefinitely busy when loading is cancelled by
     * user)
     */
    bool m_busy;

    wxString m_vfsurl;

    //We use this flag to stop recursion when we load a page from the navigation
    //callback, mainly when loading a VFS page
    bool m_guard;
    //This flag is use to indicate when a navigation event is the result of a
    //create-web-view signal and so we need to send a new window event
    bool m_creating;

protected:
    virtual void DoSetPage(const wxString& html, const wxString& baseUrl) wxOVERRIDE;

    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const wxOVERRIDE;

private:

    void ZoomIn();
    void ZoomOut();
    void SetWebkitZoom(float level);
    float GetWebkitZoom() const;

    //Find helper function
    void FindClear();

    // focus event handler: calls GTKUpdateBitmap()
    void GTKOnFocus(wxFocusEvent& event);

    WebKitWebView *m_web_view;
    int m_historyLimit;

    wxVector<wxSharedPtr<wxWebViewHandler> > m_handlerList;

    //variables used for Find()
    int m_findFlags;
    wxString m_findText;
    int m_findPosition;
    int m_findCount;

    wxDECLARE_DYNAMIC_CLASS(wxWebViewWebKit);
};

class WXDLLIMPEXP_WEBVIEW wxWebViewFactoryWebKit : public wxWebViewFactory
{
public:
    virtual wxWebView* Create() wxOVERRIDE { return new wxWebViewWebKit; }
    virtual wxWebView* Create(wxWindow* parent,
                              wxWindowID id,
                              const wxString& url = wxWebViewDefaultURLStr,
                              const wxPoint& pos = wxDefaultPosition,
                              const wxSize& size = wxDefaultSize,
                              long style = 0,
                              const wxString& name = wxWebViewNameStr) wxOVERRIDE
    { return new wxWebViewWebKit(parent, id, url, pos, size, style, name); }
};


#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT && defined(__WXGTK__)

#endif
