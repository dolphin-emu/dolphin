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

#ifndef _PLUGIN_DVD_H
#define _PLUGIN_DVD_H

#include "PluginSpecs_DVD.h"

namespace PluginDVD
{

//! IsLoaded
bool IsLoaded();

//! LoadPlugin
bool LoadPlugin(const char *_strFilename);

//! UnloadPlugin
void UnloadPlugin();

//
// --- Plugin Functions ---
//

//! GetDllInfo
void GetDllInfo(PLUGIN_INFO* _PluginInfo) ;

//! DllConfig
void DllConfig(HWND _hParent);

//! DVD_Initialize	
void DVD_Initialize(SDVDInitialize _DVDInitialize);

//! DVD_Shutdown
void DVD_Shutdown();

//! SetISOFile
void DVD_SetISOFile(const char* _szFilename);

//! GetISOName
BOOL DVD_GetISOName(TCHAR * _szFilename, int maxlen);

//! DVDReadToPtr
bool DVD_ReadToPtr(LPBYTE ptr, u64 _dwOffset, u64 _dwLength);

//! DVD_IsValid
bool DVD_IsValid();

//! DVDRead32
u32 DVD_Read32(u64 _dwOffset);

//! SaveLoadState
u32 SaveLoadState(char *ptr, BOOL save);

} // end of namespace PluginDVD

#endif
