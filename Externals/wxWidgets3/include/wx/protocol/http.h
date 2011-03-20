/////////////////////////////////////////////////////////////////////////////
// Name:        wx/protocol/http.h
// Purpose:     HTTP protocol
// Author:      Guilhem Lavaux
// Modified by: Simo Virokannas (authentication, Dec 2005)
// Created:     August 1997
// RCS-ID:      $Id: http.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 1997, 1998 Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_HTTP_H
#define _WX_HTTP_H

#include "wx/defs.h"

#if wxUSE_PROTOCOL_HTTP

#include "wx/hashmap.h"
#include "wx/protocol/protocol.h"

class WXDLLIMPEXP_NET wxHTTP : public wxProtocol
{
public:
    wxHTTP();
    virtual ~wxHTTP();

    virtual bool Connect(const wxString& host, unsigned short port);
    virtual bool Connect(const wxString& host) { return Connect(host, 0); }
    virtual bool Connect(const wxSockAddress& addr, bool wait);
    bool Abort();

    wxInputStream *GetInputStream(const wxString& path);

    wxString GetContentType() const;
    wxString GetHeader(const wxString& header) const;
    int GetResponse() const { return m_http_response; }

    void SetHeader(const wxString& header, const wxString& h_data);
    void SetPostBuffer(const wxString& post_buf);
    void SetProxyMode(bool on);

    /* Cookies */
    wxString GetCookie(const wxString& cookie) const;
    bool HasCookies() const { return m_cookies.size() > 0; }

protected:
    enum wxHTTP_Req
    {
        wxHTTP_GET,
        wxHTTP_POST,
        wxHTTP_HEAD
    };

    typedef wxStringToStringHashMap::iterator wxHeaderIterator;
    typedef wxStringToStringHashMap::const_iterator wxHeaderConstIterator;
    typedef wxStringToStringHashMap::iterator wxCookieIterator;
    typedef wxStringToStringHashMap::const_iterator wxCookieConstIterator;

    bool BuildRequest(const wxString& path, wxHTTP_Req req);
    void SendHeaders();
    bool ParseHeaders();

    wxString GenerateAuthString(const wxString& user, const wxString& pass) const;

    // find the header in m_headers
    wxHeaderIterator FindHeader(const wxString& header);
    wxHeaderConstIterator FindHeader(const wxString& header) const;
    wxCookieIterator FindCookie(const wxString& cookie);
    wxCookieConstIterator FindCookie(const wxString& cookie) const;

    // deletes the header value strings
    void ClearHeaders();
    void ClearCookies();

    // internal variables:

    wxStringToStringHashMap m_cookies;

    wxStringToStringHashMap m_headers;
    bool m_read,
         m_proxy_mode;
    wxSockAddress *m_addr;
    wxString m_post_buf;
    int m_http_response;

    DECLARE_DYNAMIC_CLASS(wxHTTP)
    DECLARE_PROTOCOL(wxHTTP)
    wxDECLARE_NO_COPY_CLASS(wxHTTP);
};

#endif // wxUSE_PROTOCOL_HTTP

#endif // _WX_HTTP_H

