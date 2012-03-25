/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/osx/webviewhistoryitem.h
// Purpose:     wxWebViewHistoryItem header for OSX
// Author:      Steven Lamerton
// Id:          $Id: webviewhistoryitem_webkit.h 69074 2011-09-12 18:35:39Z SJL $
// Copyright:   (c) 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_WEBVIEWHISTORYITEM_H_
#define _WX_OSX_WEBVIEWHISTORYITEM_H_

#include "wx/setup.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT && (defined(__WXOSX_COCOA__) \
                                          ||  defined(__WXOSX_CARBON__))

class WXDLLIMPEXP_WEBVIEW wxWebViewHistoryItem
{
public:
    wxWebViewHistoryItem(const wxString& url, const wxString& title) :
                     m_url(url), m_title(title) {}
    wxString GetUrl() { return m_url; }
    wxString GetTitle() { return m_title; }

    friend class wxWebViewWebKit;

private:
    wxString m_url, m_title;
    struct objc_object *m_histItem;
};

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT 

#endif // _WX_OSX_WEBVIEWHISTORYITEM_H_
