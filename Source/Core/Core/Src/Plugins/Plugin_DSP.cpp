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
#include "Plugin_DSP.h"

namespace PluginDSP
{

// Function Pointer
TGetDllInfo                    GetDllInfo = 0;
TDllConfig                    DllConfig = 0;
TDllDebugger                DllDebugger = 0;
TDSP_Initialize                DSP_Initialize = 0;
TDSP_Shutdown                DSP_Shutdown = 0;
TDSP_ReadMailBox            DSP_ReadMailboxHigh = 0;
TDSP_ReadMailBox            DSP_ReadMailboxLow = 0;
TDSP_WriteMailBox            DSP_WriteMailboxHigh = 0;
TDSP_WriteMailBox           DSP_WriteMailboxLow = 0;
TDSP_ReadControlRegister    DSP_ReadControlRegister = 0;
TDSP_WriteControlRegister   DSP_WriteControlRegister = 0;
TDSP_Update                    DSP_Update = 0;
TDSP_SendAIBuffer            DSP_SendAIBuffer = 0;
TDSP_DoState                DSP_DoState = 0;

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
    DllConfig = 0;
    DllDebugger = 0;
    DSP_Initialize = 0;
    DSP_Shutdown = 0;
    DSP_ReadMailboxHigh = 0;
    DSP_ReadMailboxLow = 0;
    DSP_WriteMailboxHigh = 0;
    DSP_WriteMailboxLow = 0;
    DSP_ReadControlRegister = 0;
    DSP_WriteControlRegister = 0;
    DSP_Update = 0;
    DSP_SendAIBuffer = 0;
    DSP_DoState = 0;
}

bool LoadPlugin(const char *_Filename)
{
    if (plugin.Load(_Filename)) 
    {
        GetDllInfo               = reinterpret_cast<TGetDllInfo>               (plugin.Get("GetDllInfo"));
        DllConfig                = reinterpret_cast<TDllConfig>                (plugin.Get("DllConfig"));
        DllDebugger              = reinterpret_cast<TDllDebugger>              (plugin.Get("DllDebugger"));        
        DSP_Initialize           = reinterpret_cast<TDSP_Initialize>           (plugin.Get("DSP_Initialize"));
        DSP_Shutdown             = reinterpret_cast<TDSP_Shutdown>             (plugin.Get("DSP_Shutdown"));
        DSP_ReadMailboxHigh      = reinterpret_cast<TDSP_ReadMailBox>          (plugin.Get("DSP_ReadMailboxHigh"));
        DSP_ReadMailboxLow       = reinterpret_cast<TDSP_ReadMailBox>          (plugin.Get("DSP_ReadMailboxLow"));
        DSP_WriteMailboxHigh     = reinterpret_cast<TDSP_WriteMailBox>         (plugin.Get("DSP_WriteMailboxHigh"));
        DSP_WriteMailboxLow      = reinterpret_cast<TDSP_WriteMailBox>         (plugin.Get("DSP_WriteMailboxLow"));
        DSP_ReadControlRegister  = reinterpret_cast<TDSP_ReadControlRegister>  (plugin.Get("DSP_ReadControlRegister"));
        DSP_WriteControlRegister = reinterpret_cast<TDSP_WriteControlRegister> (plugin.Get("DSP_WriteControlRegister"));
        DSP_Update               = reinterpret_cast<TDSP_Update>               (plugin.Get("DSP_Update"));
        DSP_SendAIBuffer         = reinterpret_cast<TDSP_SendAIBuffer>         (plugin.Get("DSP_SendAIBuffer"));
        DSP_DoState              = reinterpret_cast<TDSP_DoState>              (plugin.Get("DSP_DoState"));

        if ((GetDllInfo != 0) &&
            (DSP_Initialize != 0) &&
            (DSP_Shutdown != 0) &&
            (DSP_ReadMailboxHigh != 0) &&
            (DSP_ReadMailboxLow != 0) &&
            (DSP_WriteMailboxHigh != 0) &&
            (DSP_WriteMailboxLow != 0) &&
            (DSP_ReadControlRegister != 0) &&
            (DSP_WriteControlRegister != 0) &&
            (DSP_Update != 0) &&
            (DSP_SendAIBuffer != 0) &&
			(DSP_DoState != 0))
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

}  // namespace
