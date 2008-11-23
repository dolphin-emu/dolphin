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

#include "Common.h"
#include "DynamicLibrary.h"
#include "Plugin_Video.h"
#include "Plugin.h"

extern DynamicLibrary Common::CPlugin;

namespace PluginVideo
{

// Function Pointer
TGetDllInfo         GetDllInfo            = 0;
TDllConfig          DllConfig             = 0;
TDllDebugger        DllDebugger           = 0;
TVideo_Initialize   Video_Initialize      = 0;
TVideo_Prepare      Video_Prepare         = 0;
TVideo_Shutdown     Video_Shutdown        = 0;
TVideo_SendFifoData Video_SendFifoData    = 0;
TVideo_UpdateXFB    Video_UpdateXFB       = 0;
TVideo_Screenshot   Video_Screenshot      = 0;
TVideo_EnterLoop    Video_EnterLoop       = 0;
TVideo_AddMessage   Video_AddMessage      = 0;
TVideo_DoState      Video_DoState         = 0;
TVideo_Stop         Video_Stop            = 0;

// Library Instance
DynamicLibrary plugin;

void Debug(HWND _hwnd, bool Show)
{
    DllDebugger(_hwnd, Show);
}

bool IsLoaded()
{
	return plugin.IsLoaded();
}

void UnloadPlugin()
{
	//PanicAlert("Video UnloadPlugin");

    // set Functions to 0
    GetDllInfo = 0;
    DllConfig = 0;
	DllDebugger = 0;
    Video_Initialize = 0;
    Video_Prepare = 0;
    Video_Shutdown = 0;
    Video_SendFifoData = 0;
    Video_UpdateXFB = 0;
    Video_AddMessage = 0;
    Video_DoState = 0;
    Video_Stop = 0;

    plugin.Unload();
}

// ==============================================
/* Load the plugin, but first check if we have already loaded the plugin for
   the sake of showing the debugger.

   ret values:
		0 = failed
		1 = loaded successfully
		2 = already loaded from PluginManager.cpp, use it as it is */
// ------------
bool LoadPlugin(const char *_Filename)
{
	int ret = plugin.Load(_Filename);

	if (ret == 1)
	{
		GetDllInfo         = reinterpret_cast<TGetDllInfo>         (plugin.Get("GetDllInfo"));
		DllConfig          = reinterpret_cast<TDllConfig>          (plugin.Get("DllConfig"));
		DllDebugger        = reinterpret_cast<TDllDebugger>        (plugin.Get("DllDebugger"));
		Video_Initialize   = reinterpret_cast<TVideo_Initialize>   (plugin.Get("Video_Initialize"));
		Video_Prepare      = reinterpret_cast<TVideo_Prepare>      (plugin.Get("Video_Prepare"));
		Video_Shutdown     = reinterpret_cast<TVideo_Shutdown>     (plugin.Get("Video_Shutdown"));
		Video_SendFifoData = reinterpret_cast<TVideo_SendFifoData> (plugin.Get("Video_SendFifoData"));
		Video_UpdateXFB    = reinterpret_cast<TVideo_UpdateXFB>    (plugin.Get("Video_UpdateXFB"));
		Video_Screenshot   = reinterpret_cast<TVideo_Screenshot>   (plugin.Get("Video_Screenshot"));
		Video_EnterLoop    = reinterpret_cast<TVideo_EnterLoop>    (plugin.Get("Video_EnterLoop"));
		Video_AddMessage   = reinterpret_cast<TVideo_AddMessage>   (plugin.Get("Video_AddMessage"));
		Video_DoState      = reinterpret_cast<TVideo_DoState>      (plugin.Get("Video_DoState"));
        Video_Stop         = reinterpret_cast<TVideo_Stop>         (plugin.Get("Video_Stop"));
		if ((GetDllInfo != 0) &&
			(DllConfig != 0) &&
			(DllDebugger != 0) &&
			(Video_Initialize != 0) &&
			(Video_Prepare != 0) &&
			(Video_Shutdown != 0) &&
			(Video_SendFifoData != 0) &&
			(Video_UpdateXFB != 0) &&
			(Video_EnterLoop != 0) &&
			(Video_Screenshot != 0) &&
			(Video_AddMessage != 0) &&
			(Video_DoState != 0) && 
            (Video_Stop != 0))
		{
			//PanicAlert("return true: %i", ret);
			return true;
		}
		else
		{
			UnloadPlugin();
			return false;
		}
	}
	else if(ret == 2)
	{
		//PanicAlert("return true: %i", ret);
		return true;
	}
	else if(ret == 0)
	{
		//PanicAlert("return false: %i", ret);
		return false;
	}
}
// ============


}  // namespace
