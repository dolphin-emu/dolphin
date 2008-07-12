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
//! Function Types
typedef void (__cdecl* TGetDllInfo)(PLUGIN_INFO*);
typedef void (__cdecl* TDllAbout)(HWND);
typedef void (__cdecl* TDllConfig)(HWND);
typedef void (__cdecl* TDllDebugger)(HWND);
typedef void (__cdecl* TDSP_Initialize)(DSPInitialize);
typedef void (__cdecl* TDSP_Shutdown)();
typedef void (__cdecl* TDSP_WriteMailBox)(BOOL _CPUMailbox, unsigned short);
typedef unsigned short (__cdecl* TDSP_ReadMailBox)(BOOL _CPUMailbox);
typedef unsigned short (__cdecl* TDSP_ReadControlRegister)();
typedef unsigned short (__cdecl* TDSP_WriteControlRegister)(unsigned short);
typedef void (__cdecl* TDSP_Update)();
typedef void (__cdecl* TDSP_SendAIBuffer)(unsigned int _Address, unsigned int _Size);

//! Function Pointer
TGetDllInfo					g_GetDllInfo				= 0;
TDllAbout					g_DllAbout					= 0;
TDllConfig					g_DllConfig					= 0;
TDllDebugger				g_DllDebugger				= 0;
TDSP_Initialize				g_DSP_Initialize			= 0;
TDSP_Shutdown				g_DSP_Shutdown				= 0;
TDSP_ReadMailBox			g_DSP_ReadMailboxHigh		= 0;
TDSP_ReadMailBox			g_DSP_ReadMailboxLow		= 0;
TDSP_WriteMailBox			g_DSP_WriteMailboxHigh		= 0;
TDSP_WriteMailBox           g_DSP_WriteMailboxLow		= 0;
TDSP_ReadControlRegister    g_DSP_ReadControlRegister	= 0;
TDSP_WriteControlRegister   g_DSP_WriteControlRegister	= 0;
TDSP_Update					g_DSP_Update				= 0;
TDSP_SendAIBuffer			g_DSP_SendAIBuffer			= 0;

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
    g_GetDllInfo				= 0;
    g_DllAbout					= 0;	
	g_DllConfig					= 0;
	g_DllDebugger				= 0;
    g_DSP_Initialize			= 0;
    g_DSP_Shutdown				= 0;
	g_DSP_ReadMailboxHigh		= 0;
	g_DSP_ReadMailboxLow		= 0;
	g_DSP_WriteMailboxHigh		= 0;
	g_DSP_WriteMailboxLow		= 0;
	g_DSP_ReadControlRegister	= 0;
	g_DSP_WriteControlRegister	= 0;
	g_DSP_Update				= 0;
	g_DSP_SendAIBuffer			= 0;
}

bool LoadPlugin(const char *_Filename)
{
	if (plugin.Load(_Filename)) 
	{
		g_GetDllInfo				= reinterpret_cast<TGetDllInfo>					(plugin.Get("GetDllInfo"));
		g_DllAbout					= reinterpret_cast<TDllAbout>					(plugin.Get("DllAbout"));
		g_DllConfig					= reinterpret_cast<TDllConfig>					(plugin.Get("DllConfig"));
		g_DllDebugger				= reinterpret_cast<TDllDebugger>				(plugin.Get("DllDebugger"));		
		g_DSP_Initialize			= reinterpret_cast<TDSP_Initialize>				(plugin.Get("DSP_Initialize"));
		g_DSP_Shutdown				= reinterpret_cast<TDSP_Shutdown>				(plugin.Get("DSP_Shutdown"));
		g_DSP_ReadMailboxHigh		= reinterpret_cast<TDSP_ReadMailBox>			(plugin.Get("DSP_ReadMailboxHigh"));
		g_DSP_ReadMailboxLow		= reinterpret_cast<TDSP_ReadMailBox>            (plugin.Get("DSP_ReadMailboxLow"));
		g_DSP_WriteMailboxHigh		= reinterpret_cast<TDSP_WriteMailBox>           (plugin.Get("DSP_WriteMailboxHigh"));
		g_DSP_WriteMailboxLow		= reinterpret_cast<TDSP_WriteMailBox>           (plugin.Get("DSP_WriteMailboxLow"));
		g_DSP_ReadControlRegister	= reinterpret_cast<TDSP_ReadControlRegister>	(plugin.Get("DSP_ReadControlRegister"));
		g_DSP_WriteControlRegister	= reinterpret_cast<TDSP_WriteControlRegister>	(plugin.Get("DSP_WriteControlRegister"));
		g_DSP_Update				= reinterpret_cast<TDSP_Update>					(plugin.Get("DSP_Update"));
		g_DSP_SendAIBuffer			= reinterpret_cast<TDSP_SendAIBuffer>			(plugin.Get("DSP_SendAIBuffer"));

        if ((g_GetDllInfo != 0) &&
            (g_DSP_Initialize != 0) &&
            (g_DSP_Shutdown != 0) &&
            (g_DSP_ReadMailboxHigh != 0) &&
			(g_DSP_ReadMailboxLow != 0) &&
			(g_DSP_WriteMailboxHigh != 0) &&
			(g_DSP_WriteMailboxLow != 0) &&
			(g_DSP_ReadControlRegister != 0) &&
			(g_DSP_WriteControlRegister != 0) &&
			(g_DSP_Update != 0) &&
			(g_DSP_SendAIBuffer != 0)
			)
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

//
// --- Plugin Functions ---
//

// __________________________________________________________________________________________________
//
void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	g_GetDllInfo(_PluginInfo);
}

// __________________________________________________________________________________________________
//
void DllAbout(HWND _hParent)
{
	if (g_DllAbout)
		g_DllAbout(_hParent);
}

// __________________________________________________________________________________________________
//
void DllConfig(HWND _hParent)
{
	if (g_DllConfig)
		g_DllConfig(_hParent);
}

// __________________________________________________________________________________________________
//
//
void DllDebugger(HWND _hParent)
{
	if (g_DllDebugger)
		g_DllDebugger(_hParent);
}

// __________________________________________________________________________________________________
//
void DSP_Initialize(DSPInitialize _dspInitialize)
{
	g_DSP_Initialize(_dspInitialize);
}

// __________________________________________________________________________________________________
//
void DSP_Shutdown()
{
	g_DSP_Shutdown();
}

// __________________________________________________________________________________________________
//
unsigned short DSP_ReadMailboxHigh(bool _CPUMailbox)
{
	return g_DSP_ReadMailboxHigh(_CPUMailbox ? TRUE : FALSE);
}

// __________________________________________________________________________________________________
//
unsigned short DSP_ReadMailboxLow(bool _CPUMailbox)
{
	return g_DSP_ReadMailboxLow(_CPUMailbox ? TRUE : FALSE);
}

// __________________________________________________________________________________________________
//
void DSP_WriteMailboxHigh(bool _CPUMailbox, unsigned short _uHighMail)
{
	return g_DSP_WriteMailboxHigh(_CPUMailbox ? TRUE : FALSE, _uHighMail);
}

// __________________________________________________________________________________________________
//
void DSP_WriteMailboxLow(bool _CPUMailbox, unsigned short _uLowMail)
{
	return g_DSP_WriteMailboxLow(_CPUMailbox ? TRUE : FALSE, _uLowMail);
}

// __________________________________________________________________________________________________
//
unsigned short DSP_WriteControlRegister(unsigned short _Value)
{
	return g_DSP_WriteControlRegister(_Value);
}

// __________________________________________________________________________________________________
//
unsigned short DSP_ReadControlRegister()
{
	return g_DSP_ReadControlRegister();
}

// __________________________________________________________________________________________________
//
void DSP_Update()
{
	g_DSP_Update();
}

// __________________________________________________________________________________________________
//
void DSP_SendAIBuffer(unsigned int _Address, unsigned int _Size)
{
	g_DSP_SendAIBuffer(_Address, _Size);
}

}
