// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <string>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/dcclient.h>
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

wxBitmap LoadResourceBitmap(const std::string& name, const wxSize& padded_size)
{
	const std::string path_base = File::GetSysDirectory() + RESOURCES_DIR + DIR_SEP + name;
	std::string path = path_base + ".png";
	double scale_factor = 1.0;
#ifdef __APPLE__
	if (wxTheApp->GetTopWindow()->GetContentScaleFactor() >= 2)
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

	if (padded_size != wxSize())
	{
		// Add padding if necessary (or crop, but images aren't supposed to be large enough to require that).
		// The image will be left-aligned and vertically centered.
		const wxSize scaled_padded_size = padded_size * scale_factor;
		image.Resize(scaled_padded_size, wxPoint(0, (scaled_padded_size.GetHeight() - image.GetHeight()) / 2));
	}

#ifdef __APPLE__
	return wxBitmap(image, -1, scale_factor);
#else
	return wxBitmap(image);
#endif
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

// NOTE: In wxWidgets 3.1.0 there is a native function (wxWindowBase::FromDIP()) which
//	is functionally similar and can be used instead.
wxPoint FromDIP(wxPoint dip_pixels, const wxWindow* context)
{
	static constexpr int CSS_PPI = 96;

	wxSize ppi;
	{
		wxClientDC dc(const_cast<wxWindow*>(context));
		ppi = dc.GetPPI();
	}

	// Clamp PPI to >= 96. We only want to scale UP, never down.
	// Scaling down will result in bad things happening (too small to read, etc).
	// [Values less than 96 are possible on Linux, since Linux actually tries to
	//  be accurate about this but monitors lie about their dimensions]
	// NOTE: OS X may just return 72 as PPI always. On OS X,
	//	wxWindow::GetContentScaleFactor() will return 2.0 for Retina mode which
	//	can be used to scale raster images, but you can't actually get the DPI.
	//	(Not that you would want to since native widgets scale themselves).
	ppi.IncTo(wxSize(CSS_PPI, CSS_PPI));

	// MulDiv rounds the result.
	if (dip_pixels.x != wxDefaultCoord)
		dip_pixels.x = wxMulDivInt32(dip_pixels.x, ppi.GetWidth(), CSS_PPI);
	if (dip_pixels.y != wxDefaultCoord)
		dip_pixels.y = wxMulDivInt32(dip_pixels.y, ppi.GetHeight(), CSS_PPI);
	return dip_pixels;
}

int FromDIP(int value, const wxWindow* context, wxOrientation direction)
{
	if (value < 1)
		return value;

	wxPoint result = FromDIP(wxPoint(value, value), context);
	if ((direction & wxBOTH) == wxBOTH)
		return std::max(result.x, result.y);
	else if (direction & wxVERTICAL)
		return result.y;
	return result.x;
}

void SetSizeRange(wxWindow* window, wxSize min_size, wxSize max_size)
{
	window->SetMinSize(wxDefaultSize);
	window->SetMaxSize(wxDefaultSize);

	wxSize best_size = window->GetBestSize();
	best_size.DecToIfSpecified(max_size);
	min_size.DecToIfSpecified(max_size);

	if (min_size.GetWidth() == wxDefaultCoord)
		best_size.SetWidth(wxDefaultCoord);
	if (min_size.GetHeight() == wxDefaultCoord)
		best_size.SetHeight(wxDefaultCoord);
	min_size.IncTo(best_size);

	window->SetMinSize(min_size);
	window->SetMaxSize(max_size);
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
