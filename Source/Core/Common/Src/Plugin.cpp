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
DynamicLibrary CPlugin::m_hInstLib;

void(__cdecl * CPlugin::m_GetDllInfo)    (PLUGIN_INFO * _PluginInfo) = 0;
void(__cdecl * CPlugin::m_DllConfig)     (HWND _hParent) = 0;
void(__cdecl * CPlugin::m_DllDebugger)   (HWND _hParent, bool Show) = 0;
void(__cdecl * CPlugin::m_SetDllGlobals) (PLUGIN_GLOBALS* _PluginGlobals) = 0;

void
CPlugin::Release(void)
{
	m_GetDllInfo = 0;
	m_DllConfig = 0;
	m_DllDebugger = 0;
	m_SetDllGlobals = 0;
	
	m_hInstLib.Unload();
}

bool
CPlugin::Load(const char* _szName)
{
	if (m_hInstLib.Load(_szName))
	{
		m_GetDllInfo = (void (__cdecl*)(PLUGIN_INFO*)) m_hInstLib.Get("GetDllInfo");
		m_DllConfig = (void (__cdecl*)(HWND)) m_hInstLib.Get("DllConfig");
		m_DllDebugger = (void (__cdecl*)(HWND, bool)) m_hInstLib.Get("DllDebugger");
		m_SetDllGlobals = (void (__cdecl*)(PLUGIN_GLOBALS*)) m_hInstLib.Get("SetDllGlobals");
		return(true);
	}

	return(false);
}


bool CPlugin::GetInfo(PLUGIN_INFO& _pluginInfo)
{
	if (m_GetDllInfo != 0)
	{
		m_GetDllInfo(&_pluginInfo);
		return(true);
	}

	return(false);
}


void CPlugin::Config(HWND _hwnd)
{
	if (m_DllConfig != 0)
	{
		m_DllConfig(_hwnd);
	}
}

void CPlugin::Debug(HWND _hwnd, bool Show)
{
	if (m_DllDebugger != 0)
	{
		m_DllDebugger(_hwnd, Show);
	}
}

void CPlugin::SetGlobals(PLUGIN_GLOBALS& _pluginGlobals)
{
	if (m_SetDllGlobals != 0)
	{
		m_SetDllGlobals(&_pluginGlobals);
	}
}

} // end of namespace Common
