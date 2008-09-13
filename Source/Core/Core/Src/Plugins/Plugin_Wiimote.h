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

#ifndef _PLUGIN_WIIMOTE_H
#define _PLUGIN_WIIMOTE_H

#include "pluginspecs_wiimote.h"

namespace PluginWiimote
{
bool IsLoaded();
bool LoadPlugin(const char *_Filename);
void UnloadPlugin();

// Function Types
typedef void (__cdecl* TGetDllInfo)(PLUGIN_INFO*);
typedef void (__cdecl* TDllAbout)(HWND);
typedef void (__cdecl* TDllConfig)(HWND);
typedef void (__cdecl* TWiimote_Initialize)(SWiimoteInitialize);
typedef void (__cdecl* TWiimote_Shutdown)();
typedef void (__cdecl* TWiimote_Output)(const void* _pData, u32 _Size);
typedef unsigned int (__cdecl* TWiimote_GetAttachedControllers)();
typedef void (__cdecl* TWiimote_DoState)(void *ptr, int mode);

// Function Pointers
extern TGetDllInfo					GetDllInfo;
extern TDllAbout					DllAbout;
extern TDllConfig					DllConfig;
extern TWiimote_Initialize				Wiimote_Initialize;
extern TWiimote_Shutdown				Wiimote_Shutdown;
extern TWiimote_Output			    Wiimote_Output;
extern TWiimote_GetAttachedControllers			    Wiimote_GetAttachedControllers;
extern TWiimote_DoState			Wiimote_DoState;

}  // end of namespace PluginWiimote

#endif	//_PLUGIN_WIIMOTE_H
