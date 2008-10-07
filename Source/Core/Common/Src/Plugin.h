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

#ifndef _PLUGIN_H
#define _PLUGIN_H

#include "Common.h"
#include "../../../PluginSpecs/PluginSpecs.h"
#include "DynamicLibrary.h"

namespace Common
{
class CPlugin
{
	public:

		static void Release(void);
		static bool Load(const char* _szName);

		static bool GetInfo(PLUGIN_INFO& _pluginInfo);

		static void Config(HWND _hwnd);
		static void About(HWND _hwnd);
		static void Debug(HWND _hwnd);


	private:

		static DynamicLibrary m_hInstLib;

		static void (__cdecl * m_GetDllInfo)(PLUGIN_INFO* _PluginInfo);
		static void (__cdecl * m_DllConfig)(HWND _hParent);
		static void (__cdecl * m_DllDebugger)(HWND _hParent);
};
} // end of namespace Common

#endif
