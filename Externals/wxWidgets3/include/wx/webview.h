/////////////////////////////////////////////////////////////////////////////
// Name:        webview.h
// Purpose:     Common interface and events for web view component
// Author:      Marianne Gagnon
// Id:          $Id: webview.h 70038 2011-12-17 23:52:40Z VZ $
// Copyright:   (c) 2010 Marianne Gagnon, 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WEB_VIEW_H_
#define _WX_WEB_VIEW_H_

#include "wx/defs.h"

#if wxUSE_WEBVIEW

#include "wx/control.h"
#include "wx/event.h"
#include "wx/sstream.h"
#include "wx/sharedptr.h"
#include "wx/vector.h"

#if defined(__WXOSX__)
    #include "wx/osx/webviewhistoryitem_webkit.h"
#elif defined(__WXGTK__)
    #include "wx/gtk/webviewhistoryitem_webkit.h"
#elif defined(__WXMSW__)
    #include "wx/msw/webviewhistoryitem_ie.h"
#else
    #error "wxWebView not implemented on this platform."
#endif

class wxFSFile;
class wxFileSystem;

enum wxWebViewZoom
{
    wxWEB_VIEW_ZOOM_TINY,
    wxWEB_VIEW_ZOOM_SMALL,
    wxWEB_VIEW_ZOOM_MEDIUM,
    wxWEB_VIEW_ZOOM_LARGE,
    wxWEB_VIEW_ZOOM_LARGEST
};

enum wxWebViewZoomType
{
    //Scales entire page, including images
    wxWEB_VIEW_ZOOM_TYPE_LAYOUT,
    wxWEB_VIEW_ZOOM_TYPE_TEXT
};

enum wxWebViewNavigationError
{
    wxWEB_NAV_ERR_CONNECTION,
    wxWEB_NAV_ERR_CERTIFICATE,
    wxWEB_NAV_ERR_AUTH,
    wxWEB_NAV_ERR_SECURITY,
    wxWEB_NAV_ERR_NOT_FOUND,
    wxWEB_NAV_ERR_REQUEST,
    wxWEB_NAV_ERR_USER_CANCELLED,
    wxWEB_NAV_ERR_OTHER
};

enum wxWebViewReloadFlags
{
    //Default, may access cache
    wxWEB_VIEW_RELOAD_DEFAULT,
    wxWEB_VIEW_RELOAD_NO_CACHE 
};

enum wxWebViewBackend
{
    wxWEB_VIEW_BACKEND_DEFAULT,
    wxWEB_VIEW_BACKEND_WEBKIT,
    wxWEB_VIEW_BACKEND_IE
};

//Base class for custom scheme handlers
class WXDLLIMPEXP_WEBVIEW wxWebViewHandler
{
public:
    wxWebViewHandler(const wxString& scheme) : m_scheme(scheme) {}
    virtual ~wxWebViewHandler() {}
    virtual wxString GetName() const { return m_scheme; }
    virtual wxFSFile* GetFile(const wxString &uri) = 0;
private:
    wxString m_scheme;
};

extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewNameStr[];
extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewDefaultURLStr[];

class WXDLLIMPEXP_WEBVIEW wxWebView : public wxControl
{
public:
    virtual ~wxWebView() {}

    virtual bool Create(wxWindow* parent,
           wxWindowID id,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxWebViewNameStr) = 0;

    static wxWebView* New(wxWebViewBackend backend = wxWEB_VIEW_BACKEND_DEFAULT);
    static wxWebView* New(wxWindow* parent,
           wxWindowID id,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           wxWebViewBackend backend = wxWEB_VIEW_BACKEND_DEFAULT,
           long style = 0,
           const wxString& name = wxWebViewNameStr);

    //General methods
    virtual wxString GetCurrentTitle() const = 0;
    virtual wxString GetCurrentURL() const = 0;
    // TODO: handle choosing a frame when calling GetPageSource()?
    virtual wxString GetPageSource() const = 0;
    virtual wxString GetPageText() const = 0;
    virtual bool IsBusy() const = 0;
    virtual bool IsEditable() const = 0;
    virtual void LoadURL(const wxString& url) = 0;
    virtual void Print() = 0;
    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler) = 0;
    virtual void Reload(wxWebViewReloadFlags flags = wxWEB_VIEW_RELOAD_DEFAULT) = 0;
    virtual void RunScript(const wxString& javascript) = 0;
    virtual void SetEditable(bool enable = true) = 0;
    virtual void SetPage(const wxString& html, const wxString& baseUrl) = 0;
    virtual void SetPage(wxInputStream& html, wxString baseUrl)
    {
        wxStringOutputStream stream;
        stream.Write(html);
        SetPage(stream.GetString(), baseUrl);
    }
    virtual void Stop() = 0;

    //History
    virtual bool CanGoBack() const = 0;
    virtual bool CanGoForward() const = 0;
    virtual void GoBack() = 0;
    virtual void GoForward() = 0;
    virtual void ClearHistory() = 0;
    virtual void EnableHistory(bool enable = true) = 0;
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory() = 0;
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory() = 0;
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item) = 0;

    //Zoom
    virtual bool CanSetZoomType(wxWebViewZoomType type) const = 0;
    virtual wxWebViewZoom GetZoom() const = 0;
    virtual wxWebViewZoomType GetZoomType() const = 0;
    virtual void SetZoom(wxWebViewZoom zoom) = 0;
    virtual void SetZoomType(wxWebViewZoomType zoomType) = 0;

    //Selection
    virtual void SelectAll() = 0;
    virtual bool HasSelection() const = 0;
    virtual void DeleteSelection() = 0;
    virtual wxString GetSelectedText() const = 0;
    virtual wxString GetSelectedSource() const = 0;
    virtual void ClearSelection() = 0;

    //Clipboard functions
    virtual bool CanCut() const = 0;
    virtual bool CanCopy() const = 0;
    virtual bool CanPaste() const = 0;
    virtual void Cut() = 0;
    virtual void Copy() = 0;
    virtual void Paste() = 0;

    //Undo / redo functionality
    virtual bool CanUndo() const = 0;
    virtual bool CanRedo() const = 0;
    virtual void Undo() = 0;
    virtual void Redo() = 0;

    wxDECLARE_ABSTRACT_CLASS(wxWebView);
};

class WXDLLIMPEXP_WEBVIEW wxWebViewEvent : public wxNotifyEvent
{
public:
    wxWebViewEvent() {}
    wxWebViewEvent(wxEventType type, int id, const wxString url,
                   const wxString target)
        : wxNotifyEvent(type, id), m_url(url), m_target(target)
    {}


    const wxString& GetURL() const { return m_url; }
    const wxString& GetTarget() const { return m_target; }

    virtual wxEvent* Clone() const { return new wxWebViewEvent(*this); }
private:
    wxString m_url;
    wxString m_target;

    wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxWebViewEvent);
};

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_WEBVIEW, wxEVT_COMMAND_WEB_VIEW_NAVIGATING, wxWebViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_WEBVIEW, wxEVT_COMMAND_WEB_VIEW_NAVIGATED, wxWebViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_WEBVIEW, wxEVT_COMMAND_WEB_VIEW_LOADED, wxWebViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_WEBVIEW, wxEVT_COMMAND_WEB_VIEW_ERROR, wxWebViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_WEBVIEW, wxEVT_COMMAND_WEB_VIEW_NEWWINDOW, wxWebViewEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_WEBVIEW, wxEVT_COMMAND_WEB_VIEW_TITLE_CHANGED, wxWebViewEvent );

typedef void (wxEvtHandler::*wxWebViewEventFunction)
             (wxWebViewEvent&);

#define wxWebViewEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxWebViewEventFunction, func)

#define EVT_WEB_VIEW_NAVIGATING(id, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_WEB_VIEW_NAVIGATING, id, \
                     wxWebViewEventHandler(fn))

#define EVT_WEB_VIEW_NAVIGATED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_WEB_VIEW_NAVIGATED, id, \
                     wxWebViewEventHandler(fn))

#define EVT_WEB_VIEW_LOADED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_WEB_VIEW_LOADED, id, \
                     wxWebViewEventHandler(fn))

#define EVT_WEB_VIEW_ERROR(id, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_WEB_VIEW_ERROR, id, \
                     wxWebViewEventHandler(fn))

#define EVT_WEB_VIEW_NEWWINDOW(id, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_WEB_VIEW_NEWWINDOW, id, \
                     wxWebViewEventHandler(fn))

#define EVT_WEB_VIEW_TITLE_CHANGED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_WEB_VIEW_TITLE_CHANGED, id, \
                     wxWebViewEventHandler(fn))

#endif // wxUSE_WEBVIEW

#endif // _WX_WEB_VIEW_H_
