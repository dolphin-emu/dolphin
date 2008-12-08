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
#ifndef _PLUGIN_DVD
#define _PLUGIN_DVD

#include <string.h>

#include "pluginspecs_pad.h"

namespace PluginPAD
{
bool IsLoaded();
bool LoadPlugin(const char * _Filename);
void UnloadPlugin();

// Function Types
typedef void (__cdecl* TGetDllInfo)(PLUGIN_INFO*);
typedef void (__cdecl* TDllConfig)(HWND);
typedef void (__cdecl* TPAD_Initialize)(SPADInitialize);
typedef void (__cdecl* TPAD_Shutdown)();
typedef void (__cdecl* TPAD_GetStatus)(u8, SPADStatus*);
typedef void (__cdecl* TPAD_Rumble)(u8, unsigned int, unsigned int);
typedef unsigned int (__cdecl* TPAD_GetAttachedPads)();

// Function Pointers
extern TGetDllInfo GetDllInfo;
extern TPAD_Shutdown PAD_Shutdown;
extern TDllConfig DllConfig;
extern TPAD_Initialize PAD_Initialize;
extern TPAD_GetStatus PAD_GetStatus;
extern TPAD_Rumble PAD_Rumble;
extern TPAD_GetAttachedPads PAD_GetAttachedPads;

} // end of namespace PluginPAD

#endif
