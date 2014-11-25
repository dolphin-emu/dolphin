// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include <wx/bitmap.h>
#include <wx/chartype.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/msgdlg.h>
#include <wx/mstream.h>
#include <wx/string.h>
#include <wx/toolbar.h>
#include <wx/utils.h>

#include "DolphinWX/WxUtils.h"

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#endif

namespace WxUtils
{

// Launch a file according to its mime type
void Launch(const std::string& filename)
{
	if (! ::wxLaunchDefaultBrowser(StrToWxStr(filename)))
	{
		// WARN_LOG
	}
}

// Launch an file explorer window on a certain path
void Explore(const std::string& path)
{
	wxString wxPath = StrToWxStr(path);
#ifndef _WIN32
	// Default to file
	if (! wxPath.Contains("://"))
	{
		wxPath = "file://" + wxPath;
	}
#endif

#ifdef __WXGTK__
	wxPath.Replace(" ", "\\ ");
#endif

	if (! ::wxLaunchDefaultBrowser(wxPath))
	{
		// WARN_LOG
	}
}

void ShowErrorDialog(const wxString& error_msg)
{
	wxMessageBox(error_msg, _("Error"), wxOK | wxICON_ERROR);
}

wxBitmap _wxGetBitmapFromMemory(const unsigned char* data, int length)
{
	wxMemoryInputStream is(data, length);
	return(wxBitmap(wxImage(is, wxBITMAP_TYPE_ANY, -1), -1));
}

wxBitmap CreateDisabledButtonBitmap(const wxBitmap& original)
{
	wxImage image = original.ConvertToImage();
	return wxBitmap(image.ConvertToDisabled(240));
}

void AddToolbarButton(wxToolBar* toolbar, int toolID, const wxString& label, const wxBitmap& bitmap, const wxString& shortHelp)
{
	// Must explicitly set the disabled button bitmap because wxWidgets
	// incorrectly desaturates it instead of lightening it.
	toolbar->AddTool(toolID, label, bitmap, WxUtils::CreateDisabledButtonBitmap(bitmap), wxITEM_NORMAL, shortHelp);
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
