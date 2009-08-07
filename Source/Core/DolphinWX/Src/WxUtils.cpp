// Copyright (C) 2003 Dolphin Project.

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

#include "Common.h"

#include <wx/wx.h>
#include <wx/string.h>

namespace WxUtils {

// Launch a file according to its mime type
void Launch(const char *filename)
{
	if (! ::wxLaunchDefaultBrowser(wxString::FromAscii(filename))) {
		// WARN_LOG
	}
}

// Launch an file explorer window on a certain path
void Explore(const char *path)
{
	wxString wxPath = wxString::FromAscii(path);
	
	// Default to file
	if (! wxPath.Contains(wxT("://"))) {
		wxPath = wxT("file://") + wxPath;
	}

	if (! ::wxLaunchDefaultBrowser(wxPath)) {
		// WARN_LOG
	}
}

bool CopySJISToString(wxString& _rDestination, const char* _src)
{
	bool returnCode = false;
#ifdef WIN32
	// HyperIris: because dolphin using "Use Multi-Byte Character Set",
	// we must convert the SJIS chars to unicode then to our windows local by hand
	u32 unicodeNameSize = MultiByteToWideChar(932, MB_PRECOMPOSED, 
		_src, (int)strlen(_src),	NULL, NULL);
	if (unicodeNameSize > 0)
	{
		u16* pUnicodeStrBuffer = new u16[unicodeNameSize + 1];
		if (pUnicodeStrBuffer)
		{
			memset(pUnicodeStrBuffer, 0, (unicodeNameSize + 1) * sizeof(u16));
			if (MultiByteToWideChar(932, MB_PRECOMPOSED, 
				_src, (int)strlen(_src),
				(LPWSTR)pUnicodeStrBuffer, unicodeNameSize))
			{

#ifdef _UNICODE
				_rDestination = (LPWSTR)pUnicodeStrBuffer;
				returnCode = true;
#else
				u32 ansiNameSize = WideCharToMultiByte(CP_ACP, 0, 
					(LPCWSTR)pUnicodeStrBuffer, unicodeNameSize,
					NULL, NULL, NULL, NULL);
				if (ansiNameSize > 0)
				{
					char* pAnsiStrBuffer = new char[ansiNameSize + 1];
					if (pAnsiStrBuffer)
					{
						memset(pAnsiStrBuffer, 0, (ansiNameSize + 1) * sizeof(char));
						if (WideCharToMultiByte(CP_ACP, 0, 
							(LPCWSTR)pUnicodeStrBuffer, unicodeNameSize,
							pAnsiStrBuffer, ansiNameSize, NULL, NULL))
						{
							_rDestination = pAnsiStrBuffer;
							returnCode = true;
						}
						delete pAnsiStrBuffer;
					}
				}
#endif
			}
			delete pUnicodeStrBuffer;
		}
	}
#else
	_rDestination = wxString(wxString(_src,wxConvLibc),wxConvUTF8);
	returnCode = true;
#endif
	return returnCode;
}

}  // namespace
