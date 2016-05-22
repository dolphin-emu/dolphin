// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <wx/gdicmn.h>
#include <wx/string.h>

class wxBitmap;
class wxToolBar;

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

}  // namespace

std::string WxStrToStr(const wxString& str);
wxString StrToWxStr(const std::string& str);
