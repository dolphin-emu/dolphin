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

#ifndef __PLUGIN_MANAGER_H_
#define __PLUGIN_MANAGER_H_

#include "Plugin.h"

class CPluginInfo
{
public:
	CPluginInfo(const char *_rFileName);
	bool IsValid() const {return(m_Valid);}
	const PLUGIN_INFO& GetPluginInfo() const {return(m_PluginInfo);}
	const std::string& GetFileName() const {return(m_FileName);}

private:
	PLUGIN_INFO m_PluginInfo;
	std::string m_FileName;
	bool m_Valid;
};

typedef std::vector<CPluginInfo>CPluginInfos;

class CPluginManager
{
public:
	static CPluginManager& GetInstance() {return(m_Instance);}
	void ScanForPlugins(wxWindow* _wxWindow);
	void OpenAbout(void* _Parent, const char *_rFilename);
	void OpenConfig(void* _Parent, const char *_rFilename);
	void OpenDebug(void* _Parent, const char *_rFilename);
	const CPluginInfos& GetPluginInfos() {return(m_PluginInfos);}

private:
	static CPluginManager m_Instance;
	bool m_Initialized;

	CPluginInfos m_PluginInfos;

	CPluginManager();
	~CPluginManager();
};


#endif
