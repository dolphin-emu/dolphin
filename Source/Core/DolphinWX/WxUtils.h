// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <wx/string.h>

class wxBitmap;
class wxToolBar;

// A shortcut to access the bitmaps
#define wxGetBitmapFromMemory(name) WxUtils::_wxGetBitmapFromMemory(name, sizeof(name))

namespace WxUtils
{

// Launch a file according to its mime type
void Launch(const std::string& filename);

// Launch an file explorer window on a certain path
void Explore(const std::string& path);

// Displays a wxMessageBox geared for errors
void ShowErrorDialog(const wxString& error_msg);

wxBitmap _wxGetBitmapFromMemory(const unsigned char* data, int length);

// From a wxBitmap, creates the corresponding disabled version for toolbar buttons
wxBitmap CreateDisabledButtonBitmap(const wxBitmap& original);

// Helper function to add a button to a toolbar
void AddToolbarButton(wxToolBar* toolbar, int toolID, const wxString& label, const wxBitmap& bitmap, const wxString& shortHelp);

}  // namespace

std::string WxStrToStr(const wxString& str);
wxString StrToWxStr(const std::string& str);
