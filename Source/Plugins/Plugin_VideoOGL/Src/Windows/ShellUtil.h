// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma once

#include "stdafx.h"
#include <xstring>
#include <vector>

namespace W32Util
{
	std::string BrowseForFolder(HWND parent, char *title);
	bool BrowseForFileName (bool _bLoad, HWND _hParent, const char *_pTitle,
		const char *_pInitialFolder,const char *_pFilter,const char *_pExtension, 
		std::string& _strFileName);
	std::vector<std::string> BrowseForFileNameMultiSelect(bool _bLoad, HWND _hParent, const char *_pTitle,
		const char *_pInitialFolder,const char *_pFilter,const char *_pExtension);
}