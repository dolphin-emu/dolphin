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
	TGetDllInfo                     GetDllInfo = 0;
        TSetDllGlobals                  SetDllGlobals = 0;
	TDllConfig                      DllConfig = 0;
	TWiimote_Initialize             Wiimote_Initialize = 0;
	TWiimote_Shutdown               Wiimote_Shutdown = 0;
	TWiimote_Output                 Wiimote_ControlChannel = 0;
	TWiimote_Input                  Wiimote_InterruptChannel = 0;
	TWiimote_Update                 Wiimote_Update = 0;
	TWiimote_GetAttachedControllers Wiimote_GetAttachedControllers = 0;
	TWiimote_DoState                Wiimote_DoState = 0;

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
		SetDllGlobals = 0;
		DllConfig = 0;
		Wiimote_Initialize = 0;
		Wiimote_Shutdown = 0;
		Wiimote_ControlChannel = 0;
		Wiimote_InterruptChannel = 0;
		Wiimote_Update = 0;
		Wiimote_GetAttachedControllers = 0;
		Wiimote_DoState = 0;
	}

	bool LoadPlugin(const char *_Filename)
	{
		if (plugin.Load(_Filename)) 
		{
			GetDllInfo               = reinterpret_cast<TGetDllInfo>         (plugin.Get("GetDllInfo"));
			SetDllGlobals            = reinterpret_cast<TSetDllGlobals>      (plugin.Get("SetDllGlobals"));
			DllConfig                = reinterpret_cast<TDllConfig>          (plugin.Get("DllConfig"));
			Wiimote_Initialize       = reinterpret_cast<TWiimote_Initialize> (plugin.Get("Wiimote_Initialize"));
			Wiimote_Shutdown         = reinterpret_cast<TWiimote_Shutdown>   (plugin.Get("Wiimote_Shutdown"));
			Wiimote_ControlChannel   = reinterpret_cast<TWiimote_Output>     (plugin.Get("Wiimote_ControlChannel"));
			Wiimote_InterruptChannel = reinterpret_cast<TWiimote_Input>      (plugin.Get("Wiimote_InterruptChannel"));
			Wiimote_Update           = reinterpret_cast<TWiimote_Update>     (plugin.Get("Wiimote_Update"));
			Wiimote_GetAttachedControllers = reinterpret_cast<TWiimote_GetAttachedControllers> (plugin.Get("Wiimote_GetAttachedControllers"));
			Wiimote_DoState          = reinterpret_cast<TWiimote_DoState>    (plugin.Get("Wiimote_DoState"));


			if ((GetDllInfo != 0) &&
				(Wiimote_Initialize != 0) &&
				(Wiimote_Shutdown != 0) &&
				(Wiimote_ControlChannel != 0) &&
				(Wiimote_InterruptChannel != 0) &&
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
