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

//
// --- Plugin Functions ---
//

void GetDllInfo(PLUGIN_INFO* _pPluginInfo) ;
void DllAbout(HWND _hParent);
void DllConfig(HWND _hParent);
void PAD_Initialize(SPADInitialize _PADInitialize);
void PAD_Shutdown();
void PAD_GetStatus(BYTE _numPAD, SPADStatus* _pPADStatus);
void PAD_Rumble(BYTE _numPAD, unsigned int _uType, unsigned int _uStrength);
unsigned int PAD_GetNumberOfPads();
unsigned int SaveLoadState(char* _ptr, BOOL save);

} // end of namespace PluginPAD

#endif
