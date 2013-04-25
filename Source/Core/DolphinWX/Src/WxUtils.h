// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef WXUTILS_H
#define WXUTILS_H

#include <string>
#include <wx/string.h>

namespace WxUtils
{

// Launch a file according to its mime type
void Launch(const char *filename);

// Launch an file explorer window on a certain path
void Explore(const char *path);

}  // namespace

std::string WxStrToStr(const wxString& str);
wxString StrToWxStr(const std::string& str);

#endif // WXUTILS
