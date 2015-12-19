// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/msgdlg.h>
#include <wx/mstream.h>
#include <wx/toolbar.h>
#include <wx/utils.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"

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

wxBitmap LoadResourceBitmap(const std::string& name, bool allow_2x)
{
	const std::string path_base = File::GetSysDirectory() + RESOURCES_DIR + DIR_SEP + name;
	std::string path = path_base + ".png";
#ifdef __APPLE__
	double scale_factor = 1.0;
	if (allow_2x && wxTheApp->GetTopWindow()->GetContentScaleFactor() >= 2)
	{
		const std::string path_2x = path_base + "@2x.png";
		if (File::Exists(path_2x))
		{
			path = path_2x;
			scale_factor = 2.0;
		}
	}
#endif
	wxImage image(StrToWxStr(path), wxBITMAP_TYPE_PNG);
#ifdef __APPLE__
	return wxBitmap(image, -1, scale_factor);
#else
	return wxBitmap(image);
#endif
}

wxBitmap LoadResourceBitmapPadded(const std::string& name, const wxSize& size)
{
	wxImage image(StrToWxStr(File::GetSysDirectory() + RESOURCES_DIR + DIR_SEP + name + ".png"), wxBITMAP_TYPE_PNG);
	image.Resize(size, wxPoint(0, (size.GetHeight() - image.GetHeight()) / 2)); // Vertically centered
	return wxBitmap(image);
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
