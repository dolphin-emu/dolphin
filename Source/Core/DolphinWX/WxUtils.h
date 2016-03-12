// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <wx/gdicmn.h>
#include <wx/string.h>

class wxBitmap;
class wxToolBar;
class wxWindow;

namespace WxUtils
{

// Launch a file according to its mime type
void Launch(const std::string& filename);

// Launch an file explorer window on a certain path
void Explore(const std::string& path);

// Displays a wxMessageBox geared for errors
void ShowErrorDialog(const wxString& error_msg);

// Reads a PNG from the Resources folder
wxBitmap LoadResourceBitmap(const std::string& name, const wxSize& padded_size = wxSize());

// From a wxBitmap, creates the corresponding disabled version for toolbar buttons
wxBitmap CreateDisabledButtonBitmap(const wxBitmap& original);

// Helper function to add a button to a toolbar
void AddToolbarButton(wxToolBar* toolbar, int toolID, const wxString& label, const wxBitmap& bitmap, const wxString& shortHelp);

// Convert a coordinate from 96DPI "device independent pixel" coordinates (aka "CSS Pixels")
// to 'real' screen pixels. This is more convenient than wxDLG_UNIT for things like drawing,
// borders and element spacing.
// NOTE: This function exists natively in wxWidgets 3.1.0 as wxWindowBase::FromDIP()
wxPoint FromDIP(wxPoint dip_pixels, const wxWindow* context);
inline wxSize FromDIP(const wxSize& size, const wxWindow* context)
{
	wxPoint tmp = FromDIP(wxPoint(size.GetWidth(), size.GetHeight()), context);
	return { tmp.x, tmp.y };
}
// NOTE: wxSize/wxPoint versions should usually be used instead where possible to properly handle
//	rectangular DPIs.
int FromDIP(int value, const wxWindow* context, wxOrientation direction = wxBOTH);

// wxSizers use the minimum size when computing layout instead of best size.
// The best size is only taken when the minsize is -1,-1 (i.e. undefined).
// This means that elements with a MinSize specified will always have that
// size unless wxEXPAND-ed.
// This function increases MinSize up to BestSize while clamping to the
// given MaxSize if specified.
void SetSizeRange(wxWindow* window, wxSize min_size, wxSize max_size = wxDefaultSize);
inline void SetSizeRangeDIP(wxWindow* window, const wxSize& min_size, const wxSize& max_size = wxDefaultSize)
{
	SetSizeRange(window, FromDIP(min_size, window), FromDIP(max_size, window));
}

}  // namespace

std::string WxStrToStr(const wxString& str);
wxString StrToWxStr(const std::string& str);
