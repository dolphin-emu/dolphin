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

#ifndef _PLUGIN_VIDEO_H
#define _PLUGIN_VIDEO_H

#include "pluginspecs_video.h"

#include "ChunkFile.h"

namespace PluginVideo
{
bool IsLoaded();
bool LoadPlugin(const char *_Filename);
void UnloadPlugin();

//
// --- Plugin Functions ---
//

void GetDllInfo(PLUGIN_INFO* _pluginInfo);
void DllAbout(HWND _hParent);
void DllConfig(HWND _hParent);
void Video_Initialize(SVideoInitialize* _pvideoInitialize);
void Video_Prepare();
void Video_Shutdown();
void Video_EnterLoop();
void Video_SendFifoData(BYTE *_uData);
void Video_UpdateXFB(BYTE* _pXFB, DWORD _dwHeight, DWORD _dwWidth);
bool Video_Screenshot(TCHAR* _szFilename);
void Video_AddMessage(const char* pstr, unsigned int milliseconds);

void Video_DoState(unsigned char **ptr, int mode);

} // end of namespace PluginVideo

#endif
