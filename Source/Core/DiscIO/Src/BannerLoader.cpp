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
#include "stdafx.h"

#include "BannerLoader.h"
#include "BannerLoaderWii.h"
#include "BannerLoaderGC.h"

#include "VolumeCreator.h"
#include "FileUtil.h"

// HyperIris: dunno if this suitable, may be need move.
#ifdef WIN32
#include <Windows.h>
#endif

namespace DiscIO
{
bool IBannerLoader::CopyToStringAndCheck(std::string& _rDestination, const char* _src)
{
	static bool bValidChars[256];
	static bool bInitialized = false;

	if (!bInitialized)
	{
		for (int i = 0; i < 256; i++)
		{
			bValidChars[i] = false;
		}

		// generate valid chars
		for (unsigned char c = 0x20; c <= 0x80; c++)
		{
			bValidChars[c] = true;
		}

		bValidChars[0x0a] = true;
		bValidChars[0xa9] = true;
		bValidChars[0xe9] = true;

		bInitialized = true;
	}

	bool bResult = true;
	char destBuffer[2048];
	char* dest = destBuffer;
	const char* src = _src;

	// copy the string and check for "unknown" characters
	while (*src != 0x00)
	{
		u8 c = *src;

		if (c == 0x0a){c = 0x20;}

		if (bValidChars[c] == false)
		{
			bResult = false;
			break;
		}

		*dest = c;
		dest++;
		src++;
	}

	// finalize the string
	if (bResult)
	{
		*dest = 0x00;
	}
	else
	{
		dest[0] = ' ';
		dest[1] = 0x00;
	}

	_rDestination = destBuffer;

	return(bResult);
}

bool IBannerLoader::CopyUnicodeToString( std::string& _rDestination, const u16* _src )
{
	bool returnCode = false;
#ifdef WIN32
	if (_src)
	{
		u32 ansiNameSize = WideCharToMultiByte(CP_ACP, 0, 
			(LPCWSTR)_src, (int)wcslen((const wchar_t*)_src),
			NULL, NULL, NULL, NULL);
		if (ansiNameSize > 0)
		{
			char* pAnsiStrBuffer = new char[ansiNameSize + 1];
			if (pAnsiStrBuffer)
			{
				memset(pAnsiStrBuffer, 0, (ansiNameSize + 1) * sizeof(char));
				if (WideCharToMultiByte(CP_ACP, 0, 
					(LPCWSTR)_src, (int)wcslen((const wchar_t*)_src),
					pAnsiStrBuffer, ansiNameSize, NULL, NULL))
				{
					_rDestination = pAnsiStrBuffer;
					returnCode = true;
				}
				delete pAnsiStrBuffer;
			}
		}	
	}
#else
	// FIXME: Horribly broke on non win32
	//	_rDestination = _src;
	returnCode = false;
#endif
	return returnCode;
}


IBannerLoader* CreateBannerLoader(DiscIO::IFileSystem& _rFileSystem)
{
	if (IsVolumeWiiDisc(_rFileSystem.GetVolume()))
	{
		return(new CBannerLoaderWii(_rFileSystem));
	}

	return(new CBannerLoaderGC(_rFileSystem));
}
} // namespace

