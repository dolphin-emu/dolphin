// Copyright (C) 2003 Dolphin Project.

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
 
// --------------------------------------------------------------------------------------------
// This is the common Plugin class that links to the functions that are
// common to all plugins. This class is inherited by all plugin classes. But it's only created
// directly in PluginManager.cpp when we check if a plugin is valid or not.
// --------------------------------------------------------------------------------------------

#include "Plugin.h"

namespace Common
{

CPlugin::~CPlugin()
{
	m_hInstLib.Unload();
}

CPlugin::CPlugin(const char* _szName) : valid(false) 
{
	m_GetDllInfo = NULL;
	m_DllConfig = NULL; 
	m_DllDebugger = NULL;
	m_SetDllGlobals = NULL; 
	m_Initialize = NULL;
	m_Shutdown = NULL;
	m_DoState = NULL;
		
	if (m_hInstLib.Load(_szName))
	{        
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
		
		// Check if the plugin has all the functions it shold have
		if (m_GetDllInfo != 0 &&
			m_DllConfig != 0 &&
			m_DllDebugger != 0 &&
			m_SetDllGlobals != 0 &&
			m_Initialize != 0 &&
			m_Shutdown != 0 &&
			m_DoState != 0)
			valid = true;
	} 

	// Save the filename for this plugin
	Filename = _szName;
}

void *CPlugin::LoadSymbol(const char *sym)
{
	return m_hInstLib.Get(sym);
}

// GetInfo: Get DLL info
bool CPlugin::GetInfo(PLUGIN_INFO& _pluginInfo)
{
	if (m_GetDllInfo != NULL) {
		m_GetDllInfo(&_pluginInfo);
		return true;
	}    
	return false;
}

// Config: Open the Config window
void CPlugin::Config(HWND _hwnd)
{
	if (m_DllConfig != NULL)
		m_DllConfig(_hwnd);
}

// Debug: Open the Debugging window
void CPlugin::Debug(HWND _hwnd, bool Show)
{
	if (m_DllDebugger != NULL)
		m_DllDebugger(_hwnd, Show);
}

void CPlugin::SetGlobals(PLUGIN_GLOBALS* _pluginGlobals) {
	if (m_SetDllGlobals != NULL)
		m_SetDllGlobals(_pluginGlobals);
}

void CPlugin::DoState(unsigned char **ptr, int mode) {
	if (m_DoState != NULL)
		m_DoState(ptr, mode);
}

void CPlugin::Initialize(void *init)
{
	if (m_Initialize != NULL)
		m_Initialize(init);
}

void CPlugin::Shutdown()
{
	if (m_Shutdown != NULL)
		m_Shutdown();
}

} // Namespace
