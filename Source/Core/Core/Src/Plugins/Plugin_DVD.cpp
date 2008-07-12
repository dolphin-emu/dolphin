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
#include "Plugin_DVD.h"


namespace PluginDVD
{

//! Function Types
typedef void (__cdecl* TGetDllInfo)		(PLUGIN_INFO*);
typedef void (__cdecl* TDllAbout)		(HWND);
typedef void (__cdecl* TDllConfig)		(HWND);
typedef void (__cdecl* TDVD_Initialize)	(SDVDInitialize);
typedef void (__cdecl* TDVD_Shutdown)	();
typedef void (__cdecl* TDVD_SetISOFile)	(const char*);
typedef BOOL (__cdecl* TDVD_GetISOName)	(TCHAR*, int);
typedef BOOL (__cdecl* TDVD_ReadToPtr)	(LPBYTE, unsigned __int64, unsigned __int64);
typedef BOOL (__cdecl* TDVD_IsValid)	();
typedef unsigned __int32 (__cdecl* TDVD_Read32) (unsigned __int64);

//! Function Pointer
TGetDllInfo     g_GetDllInfo        = NULL;
TDllAbout       g_DllAbout          = NULL;
TDllConfig      g_DllConfig         = NULL;
TDVD_Initialize g_DVD_Initialize    = NULL;
TDVD_Shutdown   g_DVD_Shutdown      = NULL;
TDVD_SetISOFile g_DVD_SetISOFile    = NULL;
TDVD_GetISOName g_DVD_GetISOName    = NULL;
TDVD_ReadToPtr  g_DVD_ReadToPtr     = NULL;
TDVD_Read32     g_DVD_Read32        = NULL;
TDVD_IsValid    g_DVD_IsValid       = NULL;

//! Library Instance
HINSTANCE g_hLibraryInstance = NULL;

bool IsLoaded()
{
	return (g_hLibraryInstance != NULL);
}

bool LoadPlugin(const char *_strFilename)
{
	UnloadPlugin();
	g_hLibraryInstance = LoadLibrary(_strFilename);

	if (g_hLibraryInstance)
	{
		g_GetDllInfo		= reinterpret_cast<TGetDllInfo>     (GetProcAddress(g_hLibraryInstance, "GetDllInfo"));
		g_DllAbout			= reinterpret_cast<TDllAbout>       (GetProcAddress(g_hLibraryInstance, "DllAbout"));
		g_DllConfig			= reinterpret_cast<TDllConfig>      (GetProcAddress(g_hLibraryInstance, "DllConfig"));
		g_DVD_Initialize	= reinterpret_cast<TDVD_Initialize> (GetProcAddress(g_hLibraryInstance, "DVD_Initialize"));
		g_DVD_Shutdown		= reinterpret_cast<TDVD_Shutdown>   (GetProcAddress(g_hLibraryInstance, "DVD_Shutdown"));
		g_DVD_SetISOFile	= reinterpret_cast<TDVD_SetISOFile> (GetProcAddress(g_hLibraryInstance, "DVD_SetISOFile"));
		g_DVD_GetISOName    = reinterpret_cast<TDVD_GetISOName> (GetProcAddress(g_hLibraryInstance, "DVD_GetISOName"));
		g_DVD_ReadToPtr		= reinterpret_cast<TDVD_ReadToPtr>  (GetProcAddress(g_hLibraryInstance, "DVD_ReadToPtr"));
		g_DVD_Read32		= reinterpret_cast<TDVD_Read32>     (GetProcAddress(g_hLibraryInstance, "DVD_Read32"));
        g_DVD_IsValid		= reinterpret_cast<TDVD_IsValid>     (GetProcAddress(g_hLibraryInstance, "DVD_IsValid"));

		if ((g_GetDllInfo != NULL) &&
			(g_DllAbout != NULL) &&
			(g_DllConfig != NULL) &&
			(g_DVD_Initialize != NULL) &&
			(g_DVD_Shutdown != NULL) &&
			(g_DVD_SetISOFile != NULL) &&
			(g_DVD_GetISOName != NULL) &&
			(g_DVD_ReadToPtr != NULL) &&
            (g_DVD_IsValid != NULL) &&
			(g_DVD_Read32 != NULL))
		{
			return true;
		}
		else
		{
			UnloadPlugin();
			return false;
		}
	}

	return false;
}

void UnloadPlugin()
{
	// Set Functions to NULL
	g_GetDllInfo = NULL;
	g_DllAbout = NULL;
	g_DllConfig = NULL;
	g_DVD_Initialize = NULL;
	g_DVD_Shutdown = NULL;
	g_DVD_SetISOFile = NULL;
	g_DVD_Read32 = NULL;
	g_DVD_ReadToPtr = NULL;
    g_DVD_IsValid = NULL;

    // Unload library
    if (g_hLibraryInstance != NULL)
    {
        FreeLibrary(g_hLibraryInstance);
        g_hLibraryInstance = NULL;
    }
}

//
// --- Plugin Functions ---
//

void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	g_GetDllInfo(_PluginInfo);
}

void DllAbout(HWND _hParent)
{
	g_DllAbout(_hParent);
}

void DllConfig(HWND _hParent)
{
	g_DllConfig(_hParent);
}

void DVD_Initialize(SDVDInitialize _DVDInitialize)
{
	g_DVD_Initialize(_DVDInitialize);
}

void DVD_Shutdown()
{
	g_DVD_Shutdown();
}

bool DVD_ReadToPtr(LPBYTE ptr, unsigned __int64 _dwOffset, unsigned __int64 _dwLength)
{
	return (g_DVD_ReadToPtr(ptr, _dwOffset, _dwLength) == TRUE) ? true : false;	
}

bool DVD_IsValid()
{
    return (g_DVD_IsValid() == TRUE) ? true : false;
}

unsigned __int32 DVD_Read32(unsigned __int64 _dwOffset)
{
	return g_DVD_Read32(_dwOffset);
}

void DVD_SetISOFile(const char* _szFilename)
{
	g_DVD_SetISOFile(_szFilename);
}

BOOL DVD_GetISOName(TCHAR * _szFilename, int maxlen)
{
	return g_DVD_GetISOName(_szFilename, maxlen);
}

} // end of namespace PluginDVD