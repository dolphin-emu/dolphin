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

#include "DynamicLibrary.h"
#include "Plugin_PAD.h"

namespace PluginPAD
{

//! Function Types
typedef void (__cdecl* TGetDllInfo)(PLUGIN_INFO*);
typedef void (__cdecl* TDllAbout)(HWND);
typedef void (__cdecl* TDllConfig)(HWND);
typedef void (__cdecl* TPAD_Initialize)(SPADInitialize);
typedef void (__cdecl* TPAD_Shutdown)();
typedef void (__cdecl* TPAD_GetStatus)(BYTE, SPADStatus*);
typedef void (__cdecl* TPAD_Rumble)(BYTE, unsigned int, unsigned int);
typedef unsigned int (__cdecl* TPAD_GetAttachedPads)();


//! Function Pointer
TGetDllInfo		g_GetDllInfo		= 0;
TPAD_Shutdown	g_PAD_Shutdown		= 0;
TDllAbout		g_DllAbout			= 0;
TDllConfig		g_DllConfig			= 0;
TPAD_Initialize g_PAD_Initialize	= 0;
TPAD_GetStatus	g_PAD_GetStatus		= 0;
TPAD_Rumble		g_PAD_Rumble		= 0;
TPAD_GetAttachedPads g_PAD_GetAttachedPads = 0;

//! Library Instance
DynamicLibrary plugin;

bool IsLoaded()
{
	return plugin.IsLoaded();
}

void UnloadPlugin()
{
	plugin.Unload();
	// Set Functions to 0
	g_GetDllInfo = 0;
	g_PAD_Shutdown = 0;
	g_DllAbout = 0;
	g_DllConfig = 0;
	g_PAD_Initialize = 0;
	g_PAD_GetStatus = 0;
	g_PAD_Rumble = 0;
}

bool LoadPlugin(const char *_Filename)
{
	if (plugin.Load(_Filename))
	{
		g_GetDllInfo	= reinterpret_cast<TGetDllInfo>		(plugin.Get("GetDllInfo"));
		g_DllAbout		= reinterpret_cast<TDllAbout>		(plugin.Get("DllAbout"));
		g_DllConfig		= reinterpret_cast<TDllConfig>		(plugin.Get("DllConfig"));
		g_PAD_Initialize= reinterpret_cast<TPAD_Initialize>	(plugin.Get("PAD_Initialize"));
		g_PAD_Shutdown	= reinterpret_cast<TPAD_Shutdown>	(plugin.Get("PAD_Shutdown"));
		g_PAD_GetStatus	= reinterpret_cast<TPAD_GetStatus>	(plugin.Get("PAD_GetStatus"));
		g_PAD_Rumble	= reinterpret_cast<TPAD_Rumble>		(plugin.Get("PAD_Rumble"));
		g_PAD_GetAttachedPads = reinterpret_cast<TPAD_GetAttachedPads>(plugin.Get("PAD_GetAttachedPads"));

		if ((g_GetDllInfo != 0) &&
			(g_DllAbout != 0) &&
			(g_DllConfig != 0) &&
			(g_PAD_Initialize != 0) &&
			(g_PAD_Shutdown != 0) &&
			(g_PAD_GetStatus != 0))
			//(g_PAD_Rumble != 0))
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

//
// --- Plugin Functions ---
//

void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	g_GetDllInfo(_PluginInfo);
}

void PAD_Shutdown()
{
	g_PAD_Shutdown();
}

void DllAbout(HWND _hParent)
{
	g_DllAbout(_hParent);
}

void DllConfig(HWND _hParent)
{
	g_DllConfig(_hParent);
}

void PAD_Initialize(SPADInitialize _PADInitialize)
{
	g_PAD_Initialize(_PADInitialize);
}

void PAD_GetStatus(BYTE _numPAD, SPADStatus* _pPADStatus)
{
	g_PAD_GetStatus(_numPAD, _pPADStatus);
}

void PAD_Rumble(BYTE _numPAD, unsigned int _iType, unsigned int _iStrength)
{
    if (g_PAD_Rumble)
	    g_PAD_Rumble(_numPAD, _iType, _iStrength);
}

unsigned int PAD_GetAttachedPads()
{
	if (g_PAD_GetAttachedPads)
		return g_PAD_GetAttachedPads();

	return 1;
}

} // end of namespace PluginPAD
