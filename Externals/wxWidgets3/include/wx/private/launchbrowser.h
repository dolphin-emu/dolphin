///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/launchbrowser.h
// Purpose:     Helpers for wxLaunchDefaultBrowser() implementation.
// Author:      Vadim Zeitlin
// Created:     2016-02-07
// Copyright:   (c) 2016 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_LAUNCHBROWSER_H_
#define _WX_PRIVATE_LAUNCHBROWSER_H_

// ----------------------------------------------------------------------------
// wxLaunchBrowserParams: passed to wxDoLaunchDefaultBrowser()
// ----------------------------------------------------------------------------

struct wxLaunchBrowserParams
{
    explicit wxLaunchBrowserParams(int f) : flags(f) { }

    // Return either the URL or the file depending on our scheme.
    const wxString& GetPathOrURL() const
    {
        return scheme == wxS("file") ? path : url;
    }


    // The URL is always specified and is the real URL, always with the scheme
    // part, which can be "file://".
    wxString url;

    // The path is a local path which is only non-empty if the URL uses the
    // "file://" scheme.
    wxString path;

    // The scheme of the URL, e.g. "file" or "http".
    wxString scheme;

    // The flags passed to wxLaunchDefaultBrowser().
    int flags;
};

#endif // _WX_PRIVATE_LAUNCHBROWSER_H_
