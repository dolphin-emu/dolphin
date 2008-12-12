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

// Function Types
typedef void (__cdecl* TGetDllInfo)(PLUGIN_INFO*);
typedef void (__cdecl* TDllConfig)(HWND);
typedef void (__cdecl* TDllDebugger)(HWND, bool);
typedef void (__cdecl* TDSP_Initialize)(DSPInitialize);
typedef void (__cdecl* TDSP_Shutdown)();
typedef void (__cdecl* TDSP_WriteMailBox)(bool _CPUMailbox, unsigned short);
typedef unsigned short (__cdecl* TDSP_ReadMailBox)(bool _CPUMailbox);
typedef unsigned short (__cdecl* TDSP_ReadControlRegister)();
typedef unsigned short (__cdecl* TDSP_WriteControlRegister)(unsigned short);
typedef void (__cdecl* TDSP_Update)(int cycles);
typedef void (__cdecl* TDSP_SendAIBuffer)(unsigned int address, int sample_rate);
typedef void (__cdecl* TDSP_DoState)(unsigned char **ptr, int mode);

// Function Pointers
extern TGetDllInfo					GetDllInfo;
extern TDllConfig					DllConfig;
extern TDllDebugger				    DllDebugger;
extern TDSP_Initialize				DSP_Initialize;
extern TDSP_Shutdown				DSP_Shutdown;
extern TDSP_ReadMailBox			    DSP_ReadMailboxHigh;
extern TDSP_ReadMailBox			    DSP_ReadMailboxLow;
extern TDSP_WriteMailBox			DSP_WriteMailboxHigh;
extern TDSP_WriteMailBox            DSP_WriteMailboxLow;
extern TDSP_ReadControlRegister     DSP_ReadControlRegister;
extern TDSP_WriteControlRegister    DSP_WriteControlRegister;
extern TDSP_Update                  DSP_Update;
extern TDSP_SendAIBuffer			DSP_SendAIBuffer;
extern TDSP_DoState                 DSP_DoState;

}  // end of namespace PluginDSP

#endif
