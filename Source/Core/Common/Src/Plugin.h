// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include "Common.h"
#include "PluginSpecs.h"
#include "DynamicLibrary.h"

namespace Common
{
    typedef void (__cdecl * TGetDllInfo)(PLUGIN_INFO*);
    typedef void (__cdecl * TDllConfig)(HWND);
    typedef void (__cdecl * TDllDebugger)(HWND, bool);
    typedef void (__cdecl * TSetDllGlobals)(PLUGIN_GLOBALS*);
    typedef void (__cdecl * TInitialize)(void *);
    typedef void (__cdecl * TShutdown)();
    typedef void (__cdecl * TDoState)(unsigned char**, int);

class CPlugin
{
public:
	CPlugin(const char* _szName);
	virtual ~CPlugin();

	// This functions is only used when CPlugin is called directly, when a parent class like PluginVideo
	// is called its own IsValid() will be called. 
	virtual bool IsValid() { return valid; };
	const std::string& GetFilename() const { return Filename; }
	bool GetInfo(PLUGIN_INFO& _pluginInfo);
	void SetGlobals(PLUGIN_GLOBALS* _PluginGlobals);
	void *LoadSymbol(const char *sym);

	void Config(HWND _hwnd);
	void About(HWND _hwnd);
	void Debug(HWND _hwnd, bool Show);
	void DoState(unsigned char **ptr, int mode);
	void Initialize(void *init);
	void Shutdown();

private:
	DynamicLibrary m_hInstLib;
	std::string Filename;
	bool valid;

	// Functions
	TGetDllInfo m_GetDllInfo;
	TDllConfig m_DllConfig;
	TDllDebugger m_DllDebugger;
	TSetDllGlobals m_SetDllGlobals;
	TInitialize m_Initialize;
	TShutdown m_Shutdown;
	TDoState m_DoState;
};
} // Namespace

#endif // _PLUGIN_H_
