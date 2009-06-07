// Copyright (C) 2003-2009 Dolphin Project.

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
		for (int i = 0x20; i < 256; i++)
		{
			bValidChars[i] = true;
		}

		bValidChars[0x0a] = true;
		//bValidChars[0xa9] = true;
		//bValidChars[0xe9] = true;

		bInitialized = true;
	}

	bool bResult = true;
	char destBuffer[2048] = {0};
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

bool IBannerLoader::CopyBeUnicodeToString( std::string& _rDestination, const u16* _src, int length )
{
	bool returnCode = false;
#ifdef WIN32
	if (_src)
	{
		u16* buffer = new u16[length];
		if (buffer)
		{
			memcpy(buffer, _src, sizeof(u16)*length);
			for (int i = 0; i < length; i++)
			{
				buffer[i] = swap16(buffer[i]);
			}
			
			u32 ansiNameSize = WideCharToMultiByte(932, 0, 
				(LPCWSTR)buffer, (int)wcslen((LPCWSTR)buffer),
				NULL, NULL, NULL, NULL);
			if (ansiNameSize > 0)
			{
				char* pAnsiStrBuffer = new char[ansiNameSize + 1];
				if (pAnsiStrBuffer)
				{
					memset(pAnsiStrBuffer, 0, (ansiNameSize + 1) * sizeof(char));
					if (WideCharToMultiByte(932, 0, 
						(LPCWSTR)buffer, (int)wcslen((LPCWSTR)buffer),
						pAnsiStrBuffer, ansiNameSize, NULL, NULL))
					{
						_rDestination = pAnsiStrBuffer;
						returnCode = true;
					}
					delete pAnsiStrBuffer;
				}
			}	
			delete buffer;
		}
	}
#else
	// FIXME: Horribly broke on non win32
	//	_rDestination = _src;
	returnCode = false;
#endif
	return returnCode;
}


IBannerLoader* CreateBannerLoader(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume *pVolume)
{
	if (IsVolumeWiiDisc(pVolume) || IsVolumeWadFile(pVolume))
	{
		return(new CBannerLoaderWii(pVolume));
	}

	return(new CBannerLoaderGC(_rFileSystem));
}
} // namespace

