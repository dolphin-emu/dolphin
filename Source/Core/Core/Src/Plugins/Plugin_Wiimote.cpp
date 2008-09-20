// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"
#include "DynamicLibrary.h"
#include "Plugin_Wiimote.h"

namespace PluginWiimote
{

	// Function Pointer
	TGetDllInfo GetDllInfo = 0;
	TDllAbout DllAbout = 0;
	TDllConfig DllConfig = 0;
	TWiimote_Initialize Wiimote_Initialize = 0;
	TWiimote_Shutdown Wiimote_Shutdown = 0;
	TWiimote_Output Wiimote_Output = 0;
	TWiimote_Update Wiimote_Update = 0;
	TWiimote_GetAttachedControllers Wiimote_GetAttachedControllers = 0;
	TWiimote_DoState Wiimote_DoState = 0;

	//! Library Instance
	DynamicLibrary plugin;

	bool IsLoaded()
	{
		return plugin.IsLoaded();
	}

	void UnloadPlugin()
	{
		plugin.Unload();

		// Set Functions to NULL
		GetDllInfo = 0;
		DllAbout = 0;
		DllConfig = 0;
		Wiimote_Initialize = 0;
		Wiimote_Shutdown = 0;
		Wiimote_Output = 0;
		Wiimote_Update = 0;
		Wiimote_GetAttachedControllers = 0;
		Wiimote_DoState = 0;
	}

	bool LoadPlugin(const char *_Filename)
	{
		if (plugin.Load(_Filename)) 
		{
			LOG(MASTER_LOG, "getting Wiimote Plugin function pointers...");
			GetDllInfo = reinterpret_cast<TGetDllInfo> (plugin.Get("GetDllInfo"));
			DllAbout = reinterpret_cast<TDllAbout> (plugin.Get("DllAbout"));
			DllConfig = reinterpret_cast<TDllConfig> (plugin.Get("DllConfig"));
			Wiimote_Initialize = reinterpret_cast<TWiimote_Initialize> (plugin.Get("Wiimote_Initialize"));
			Wiimote_Shutdown = reinterpret_cast<TWiimote_Shutdown> (plugin.Get("Wiimote_Shutdown"));
			Wiimote_Output = reinterpret_cast<TWiimote_Output> (plugin.Get("Wiimote_Output"));
			Wiimote_Update = reinterpret_cast<TWiimote_Update> (plugin.Get("Wiimote_Update"));
			Wiimote_GetAttachedControllers = reinterpret_cast<TWiimote_GetAttachedControllers> (plugin.Get("Wiimote_GetAttachedControllers"));
			Wiimote_DoState = reinterpret_cast<TWiimote_DoState> (plugin.Get("Wiimote_DoState"));

			LOG(MASTER_LOG, "%s: 0x%p", "GetDllInfo", GetDllInfo);
			LOG(MASTER_LOG, "%s: 0x%p", "DllAbout", DllAbout);
			LOG(MASTER_LOG, "%s: 0x%p", "DllConfig", DllConfig);
			LOG(MASTER_LOG, "%s: 0x%p", "Wiimote_Initialize", Wiimote_Initialize);
			LOG(MASTER_LOG, "%s: 0x%p", "Wiimote_Shutdown", Wiimote_Shutdown);
			LOG(MASTER_LOG, "%s: 0x%p", "Wiimote_Output", Wiimote_Output);
			LOG(MASTER_LOG, "%s: 0x%p", "Wiimote_Update", Wiimote_Update);
			LOG(MASTER_LOG, "%s: 0x%p", "Wiimote_GetAttachedControllers", Wiimote_GetAttachedControllers);
			LOG(MASTER_LOG, "%s: 0x%p", "Wiimote_DoState", Wiimote_DoState);
			if ((GetDllInfo != 0) &&
				(Wiimote_Initialize != 0) &&
				(Wiimote_Shutdown != 0) &&
				(Wiimote_Output != 0) &&
				(Wiimote_Update != 0) &&
				(Wiimote_GetAttachedControllers != 0) &&
				(Wiimote_DoState != 0))
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

} // namespace
