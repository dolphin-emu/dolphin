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

#ifndef _PLUGIN_DSP_H
#define _PLUGIN_DSP_H

#include "pluginspecs_dsp.h"

namespace PluginDSP
{
bool IsLoaded();
bool LoadPlugin(const char *_Filename);
void UnloadPlugin();

//
// --- Plugin Functions ---
//

void GetDllInfo (PLUGIN_INFO* _PluginInfo) ;
void DllAbout (HWND _hParent);
void DllConfig (HWND _hParent);
void DllDebugger(HWND _hParent);
void DSP_Initialize(DSPInitialize _dspInitialize);
void DSP_Shutdown();
unsigned short DSP_ReadMailboxHigh(bool _CPUMailbox);
unsigned short DSP_ReadMailboxLow(bool _CPUMailbox);
void DSP_WriteMailboxHigh(bool _CPUMailbox, unsigned short _Value);
void DSP_WriteMailboxLow(bool _CPUMailbox, unsigned short _Value);
unsigned short DSP_WriteControlRegister(unsigned short _Flags);
unsigned short DSP_ReadControlRegister();
void DSP_Update(int cycles);
void DSP_SendAIBuffer(unsigned int address, int sample_rate);

}  // end of namespace PluginDSP

#endif
