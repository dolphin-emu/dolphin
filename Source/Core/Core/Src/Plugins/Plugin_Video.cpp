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
#include "Plugin_Video.h"

namespace PluginVideo
{

//! Function Types
typedef void (__cdecl* TGetDllInfo)(PLUGIN_INFO*);
typedef void (__cdecl* TDllAbout)(HWND);
typedef void (__cdecl* TDllConfig)(HWND);
typedef void (__cdecl* TVideo_Initialize)(SVideoInitialize*);
typedef void (__cdecl* TVideo_Prepare)();
typedef void (__cdecl* TVideo_Shutdown)();
typedef void (__cdecl* TVideo_SendFifoData)(BYTE*);
typedef void (__cdecl* TVideo_UpdateXFB)(BYTE*, DWORD, DWORD);
typedef BOOL (__cdecl* TVideo_Screenshot)(TCHAR*);
typedef void (__cdecl* TVideo_EnterLoop)();

typedef void (__cdecl* TVideo_AddMessage)(const char* pstr, unsigned int milliseconds);


//! Function Pointer
TGetDllInfo         g_GetDllInfo            = 0;
TDllAbout           g_DllAbout              = 0;
TDllConfig          g_DllConfig             = 0;
TVideo_Initialize   g_Video_Initialize      = 0;
TVideo_Prepare      g_Video_Prepare         = 0;
TVideo_Shutdown     g_Video_Shutdown        = 0;
TVideo_SendFifoData g_Video_SendFifoData    = 0;
TVideo_UpdateXFB    g_Video_UpdateXFB       = 0;
TVideo_Screenshot   g_Video_Screenshot      = 0;
TVideo_EnterLoop    g_Video_EnterLoop       = 0;
TVideo_AddMessage   g_Video_AddMessage      = 0;

//! Library Instance
DynamicLibrary plugin;

bool IsLoaded()
{
	return plugin.IsLoaded();
}

void UnloadPlugin()
{
    // set Functions to 0
    g_GetDllInfo = 0;
    g_DllAbout = 0;
    g_DllConfig = 0;
    g_Video_Initialize = 0;
    g_Video_Prepare = 0;
    g_Video_Shutdown = 0;
    g_Video_SendFifoData = 0;
    g_Video_UpdateXFB = 0;
	g_Video_AddMessage = 0;

	plugin.Unload();
}

bool LoadPlugin(const char *_Filename)
{
	if (plugin.Load(_Filename))
	{
		g_GetDllInfo			= reinterpret_cast<TGetDllInfo>         (plugin.Get("GetDllInfo"));
		g_DllAbout				= reinterpret_cast<TDllAbout>           (plugin.Get("DllAbout"));
		g_DllConfig				= reinterpret_cast<TDllConfig>          (plugin.Get("DllConfig"));
		g_Video_Initialize      = reinterpret_cast<TVideo_Initialize>   (plugin.Get("Video_Initialize"));
		g_Video_Prepare			= reinterpret_cast<TVideo_Prepare>      (plugin.Get("Video_Prepare"));
		g_Video_Shutdown        = reinterpret_cast<TVideo_Shutdown>     (plugin.Get("Video_Shutdown"));
		g_Video_SendFifoData    = reinterpret_cast<TVideo_SendFifoData> (plugin.Get("Video_SendFifoData"));
		g_Video_UpdateXFB		= reinterpret_cast<TVideo_UpdateXFB>    (plugin.Get("Video_UpdateXFB"));
		g_Video_Screenshot		= reinterpret_cast<TVideo_Screenshot>   (plugin.Get("Video_Screenshot"));
		g_Video_EnterLoop       = reinterpret_cast<TVideo_EnterLoop>    (plugin.Get("Video_EnterLoop"));
		g_Video_AddMessage      = reinterpret_cast<TVideo_AddMessage>   (plugin.Get("Video_AddMessage"));

		if ((g_GetDllInfo != 0) &&
			(g_DllAbout != 0) &&
			(g_DllConfig != 0) &&
			(g_Video_Initialize != 0) &&
			(g_Video_Prepare != 0) &&
			(g_Video_Shutdown != 0) &&
			(g_Video_SendFifoData != 0) &&
			(g_Video_UpdateXFB != 0) &&
			(g_Video_EnterLoop != 0) &&
			(g_Video_Screenshot != 0) &&
			(g_Video_AddMessage != 0))
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

void GetDllInfo (PLUGIN_INFO* _PluginInfo)
{
	g_GetDllInfo(_PluginInfo);
}

void DllAbout (HWND _hParent)
{
	g_DllAbout(_hParent);
}

void DllConfig (HWND _hParent)
{
	g_DllConfig(_hParent);
}

void Video_Initialize(SVideoInitialize* _pVideoInitialize)
{
	g_Video_Initialize(_pVideoInitialize);
}

void Video_Prepare()
{
	g_Video_Prepare();
}

void Video_Shutdown()
{
	g_Video_Shutdown();
}

void Video_SendFifoData(BYTE *_uData)
{
	g_Video_SendFifoData(_uData);
}

void Video_UpdateXFB(BYTE* _pXFB, DWORD _dwHeight, DWORD _dwWidth)
{
	g_Video_UpdateXFB(_pXFB, _dwHeight, _dwWidth);
}

bool Video_Screenshot(TCHAR* _szFilename)
{
	return ((g_Video_Screenshot(_szFilename) == TRUE) ? true : false);
}

void Video_EnterLoop()
{
	g_Video_EnterLoop();
}

void Video_AddMessage(const char* pstr, unsigned int milliseconds)
{
	g_Video_AddMessage(pstr,milliseconds);
}

} // end of namespace PluginVideo
