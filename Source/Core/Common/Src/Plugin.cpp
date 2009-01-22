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

 

// =======================================================
// File description
// -------------
/* This file is a simpler version of Plugin_...cpp found in Core. This file only loads
   the config and debugging windows and works with all plugins. */
// =============

 
#include "Plugin.h"

namespace Common
{

CPlugin::~CPlugin()
{
    m_hInstLib.Unload();
}

CPlugin::CPlugin(const char* _szName) : valid(false)
{
    if (m_hInstLib.Load(_szName)) {

	m_GetDllInfo = reinterpret_cast<TGetDllInfo>
	    (m_hInstLib.Get("GetDllInfo"));
	m_DllConfig = reinterpret_cast<TDllConfig>
	    (m_hInstLib.Get("DllConfig"));
	m_DllDebugger = reinterpret_cast<TDllDebugger>
	    (m_hInstLib.Get("DllDebugger"));
	m_SetDllGlobals = reinterpret_cast<TSetDllGlobals>
	    (m_hInstLib.Get("SetDllGlobals"));
	m_Initialize = reinterpret_cast<TInitialize>
	    (m_hInstLib.Get("Initialize"));
	m_Shutdown = reinterpret_cast<TShutdown>
	    (m_hInstLib.Get("Shutdown"));
	m_DoState = reinterpret_cast<TDoState>
	    (m_hInstLib.Get("DoState"));
	}

    if (m_GetDllInfo != 0 &&
	m_DllConfig != 0 &&
	m_DllDebugger != 0 &&
	m_SetDllGlobals != 0 &&
	m_Initialize != 0 &&
	m_Shutdown != 0 &&
	m_DoState != 0)
	valid = true;

}

void *CPlugin::LoadSymbol(const char *sym) {
    return m_hInstLib.Get(sym);
}


// ______________________________________________________________________________________
// GetInfo: Get DLL info
bool CPlugin::GetInfo(PLUGIN_INFO& _pluginInfo) {
    if (m_GetDllInfo != 0)
	{
		m_GetDllInfo(&_pluginInfo);
		return(true);
    }

    return(false);
}

// ______________________________________________________________________________________
// Config: Open the Config window
void CPlugin::Config(HWND _hwnd)
{
    if (m_DllConfig != 0) m_DllConfig(_hwnd);
}


// ______________________________________________________________________________________
// Debug: Open the Debugging window
void CPlugin::Debug(HWND _hwnd, bool Show)
{
    if (m_DllDebugger != 0) m_DllDebugger(_hwnd, Show);

}

void CPlugin::SetGlobals(PLUGIN_GLOBALS* _pluginGlobals) {
    if (m_SetDllGlobals != 0)
	m_SetDllGlobals(_pluginGlobals);

}

void CPlugin::DoState(unsigned char **ptr, int mode) {
    if (m_DoState != 0)
	m_DoState(ptr, mode);
}

// Run Initialize() in the plugin
void CPlugin::Initialize(void *init)
{
	/* We first check that we have found the Initialize() function, but there is no
	   restriction on running this several times */
    if (m_Initialize != 0) m_Initialize(init);
}

void CPlugin::Shutdown()
{
    if (m_Shutdown != 0) m_Shutdown();
}

} // end of namespace Common