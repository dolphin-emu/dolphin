/////////////////////////////////////////////////////////////////////////////
// Name:        webview.cpp
// Purpose:     Common interface and events for web view component
// Author:      Marianne Gagnon
// Id:          $Id: webview.cpp 69981 2011-12-11 05:36:52Z PC $
// Copyright:   (c) 2010 Marianne Gagnon, 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_WEBVIEW

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#include "wx/webview.h"

#if defined(__WXOSX_COCOA__) || defined(__WXOSX_CARBON__)
#include "wx/osx/webview_webkit.h"
#elif defined(__WXGTK__)
#include "wx/gtk/webview_webkit.h"
#elif defined(__WXMSW__)
#include "wx/msw/webview_ie.h"
#endif

// DLL options compatibility check:
#include "wx/app.h"
WX_CHECK_BUILD_OPTIONS("wxWEBVIEW")

extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewNameStr[] = "wxWebView";
extern WXDLLIMPEXP_DATA_WEBVIEW(const char) wxWebViewDefaultURLStr[] = "about:blank";

wxIMPLEMENT_ABSTRACT_CLASS(wxWebView, wxControl);
wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewEvent, wxCommandEvent);

wxDEFINE_EVENT( wxEVT_COMMAND_WEB_VIEW_NAVIGATING, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_WEB_VIEW_NAVIGATED, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_WEB_VIEW_LOADED, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_WEB_VIEW_ERROR, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_WEB_VIEW_NEWWINDOW, wxWebViewEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_WEB_VIEW_TITLE_CHANGED, wxWebViewEvent );

// static
wxWebView* wxWebView::New(wxWebViewBackend backend)
{
    switch (backend)
    {
        #if defined(wxUSE_WEBVIEW_WEBKIT) && \
           (defined(__WXGTK__) || defined(__WXOSX__))
        case wxWEB_VIEW_BACKEND_WEBKIT:
            return new wxWebViewWebKit();
        #endif

        #if wxUSE_WEBVIEW_IE
        case wxWEB_VIEW_BACKEND_IE:
            return new wxWebViewIE();
        #endif

        case wxWEB_VIEW_BACKEND_DEFAULT:

            #if defined(wxUSE_WEBVIEW_WEBKIT) && \
               (defined(__WXGTK__) || defined(__WXOSX__))
            return new wxWebViewWebKit();
            #endif

            #if wxUSE_WEBVIEW_IE
            return new wxWebViewIE();
            #endif

        // fall-through intended
        default:
            return NULL;
    }
}

// static
wxWebView* wxWebView::New(wxWindow* parent,
       wxWindowID id,
       const wxString& url,
       const wxPoint& pos,
       const wxSize& size,
       wxWebViewBackend backend,
       long style,
       const wxString& name)
{
    switch (backend)
    {
        #if defined(wxUSE_WEBVIEW_WEBKIT) && \
           (defined(__WXGTK__) || defined(__WXOSX__))
        case wxWEB_VIEW_BACKEND_WEBKIT:
            return new wxWebViewWebKit(parent, id, url, pos, size, style, name);
        #endif

        #if wxUSE_WEBVIEW_IE
        case wxWEB_VIEW_BACKEND_IE:
            return new wxWebViewIE(parent, id, url, pos, size, style, name);
        #endif

        case wxWEB_VIEW_BACKEND_DEFAULT:

            #if defined(wxUSE_WEBVIEW_WEBKIT) && \
               (defined(__WXGTK__) || defined(__WXOSX__))
            return new wxWebViewWebKit(parent, id, url, pos, size, style, name);
            #endif

            #if wxUSE_WEBVIEW_IE
            return new wxWebViewIE(parent, id, url, pos, size, style, name);
            #endif

        // fall-through intended
        default:
            return NULL;
    }
}

#endif // wxUSE_WEBVIEW
