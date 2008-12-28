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

// Function Pointers
TGetDllInfo		GetDllInfo = 0;
TPAD_Shutdown	PAD_Shutdown = 0;
TDllConfig		DllConfig = 0;
TPAD_Initialize PAD_Initialize = 0;
TPAD_GetStatus	PAD_GetStatus = 0;
TPAD_Input		PAD_Input = 0;
TPAD_Rumble		PAD_Rumble = 0;
TPAD_GetAttachedPads PAD_GetAttachedPads = 0;

// Library Instance
DynamicLibrary plugin;

bool IsLoaded()
{
	return plugin.IsLoaded();
}

void UnloadPlugin()
{
	plugin.Unload();
	// Set Functions to 0
	GetDllInfo = 0;
	PAD_Shutdown = 0;
	DllConfig = 0;
	PAD_Initialize = 0;
	PAD_GetStatus = 0;
	PAD_Input = 0;
	PAD_Rumble = 0;
}

bool LoadPlugin(const char *_Filename)
{
	if (plugin.Load(_Filename))
	{
		GetDllInfo      = reinterpret_cast<TGetDllInfo>     (plugin.Get("GetDllInfo"));
		DllConfig       = reinterpret_cast<TDllConfig>      (plugin.Get("DllConfig"));
		PAD_Initialize  = reinterpret_cast<TPAD_Initialize> (plugin.Get("PAD_Initialize"));
		PAD_Shutdown    = reinterpret_cast<TPAD_Shutdown>   (plugin.Get("PAD_Shutdown"));
		PAD_GetStatus   = reinterpret_cast<TPAD_GetStatus>  (plugin.Get("PAD_GetStatus"));
		PAD_Input	   = reinterpret_cast<TPAD_Input>		(plugin.Get("PAD_Input"));
		PAD_Rumble      = reinterpret_cast<TPAD_Rumble>     (plugin.Get("PAD_Rumble"));
		PAD_GetAttachedPads = reinterpret_cast<TPAD_GetAttachedPads>(plugin.Get("PAD_GetAttachedPads"));

		if ((GetDllInfo != 0) &&
			(DllConfig != 0) &&
			(PAD_Initialize != 0) &&
			(PAD_Shutdown != 0) &&
			(PAD_GetStatus != 0) &&
			(PAD_Input != 0))
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

}  // end of namespace PluginPAD
