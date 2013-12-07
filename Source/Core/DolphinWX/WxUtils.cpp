// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include <wx/wx.h>
#include <wx/string.h>

#include "WxUtils.h"

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#endif

namespace WxUtils {

// Launch a file according to its mime type
void Launch(const char *filename)
{
	if (! ::wxLaunchDefaultBrowser(StrToWxStr(filename)))
	{
		// WARN_LOG
	}
}

// Launch an file explorer window on a certain path
void Explore(const char *path)
{
	wxString wxPath = StrToWxStr(path);
#ifndef _WIN32
	// Default to file
	if (! wxPath.Contains(wxT("://")))
	{
		wxPath = wxT("file://") + wxPath;
	}
#endif

#ifdef __WXGTK__
	wxPath.Replace(wxT(" "), wxT("\\ "));
#endif

	if (! ::wxLaunchDefaultBrowser(wxPath))
	{
		// WARN_LOG
	}
}

double GetCurrentBitmapLogicalScale()
{
#ifdef __APPLE__
	// wx doesn't expose this itself, unfortunately.
    if ([[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)])
	{
        return [[NSScreen mainScreen] backingScaleFactor];
	}
#endif
	return 1.0;
}

}  // namespace

std::string WxStrToStr(const wxString& str)
{
	return str.ToUTF8().data();
}

wxString StrToWxStr(const std::string& str)
{
	//return wxString::FromUTF8Unchecked(str.c_str());
	return wxString::FromUTF8(str.c_str());
}
