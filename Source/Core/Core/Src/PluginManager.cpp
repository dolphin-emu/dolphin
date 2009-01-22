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

#include <string>
#include <vector>

//#include "Globals.h"
#include "FileSearch.h"
#include "FileUtil.h"
#include "PluginManager.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "StringUtil.h"

CPluginManager CPluginManager::m_Instance;


CPluginManager::CPluginManager() : 
    m_params(SConfig::GetInstance().m_LocalCoreStartupParameter)
{
    m_PluginGlobals = new PLUGIN_GLOBALS;
    m_PluginGlobals->eventHandler = EventHandler::GetInstance();
    m_PluginGlobals->config = (void *)&SConfig::GetInstance();
    m_PluginGlobals->messageLogger = NULL;
    m_InputManager = new InputManager();

}


CPluginManager::~CPluginManager()
{
    if (m_PluginGlobals)
	delete m_PluginGlobals;

    ShutdownPlugins();

    delete m_InputManager;
    
}

bool CPluginManager::InitPlugins() {

    if (! GetVideo()) {
	PanicAlert("Can't init Video Plugin");
	return false;
    }

    if (! GetDSP()) {
	PanicAlert("Can't init DSP Plugin");
	return false;
    }

    if (! m_InputManager->Init()) {
	PanicAlert("Can't init input manager");
	return false;
    }

    bool pad = false;
    bool wiimote = false;

    for (int i = 0; i < MAXPADS; i++)
	{
	if (! m_params.m_strPadPlugin[i].empty())
	    GetPAD(i);

	if (m_pad[i] != NULL)
	    pad = true;
    }

    if (! pad) {
	PanicAlert("Can't init any PAD Plugins");
	return false;
    }
    if (m_params.bWii) {
	for (int i = 0; i < MAXWIIMOTES; i++)
	{
	    if (! m_params.m_strWiimotePlugin[i].empty())
		GetWiimote(i);

	    if (m_wiimote[i] != NULL)
		wiimote = true;
	}

	if (! wiimote) {
	    PanicAlert("Can't init any Wiimote Plugins");
	    return false;
	}
    }

    return true;
}

void CPluginManager::ShutdownPlugins()
{   
    for (int i = 0; i < MAXPADS; i++) {
	if (m_pad[i]) { 
	    m_pad[i]->Shutdown();
	    delete m_pad[i];
	    m_pad[i] = NULL;
	}
    }

   if (! m_InputManager->Shutdown()) {
       PanicAlert("Error cleaning after input manager");
   }

   for (int i = 0; i < MAXWIIMOTES; i++) {
       if (m_wiimote[i]) {
	   m_wiimote[i]->Shutdown();
	   delete m_wiimote[i];
	   m_wiimote[i] = NULL;
       }
   }

   if (m_video) {
       m_video->Shutdown();
       delete m_video;
       m_video = NULL;
   }

   if (m_dsp) {
       m_dsp->Shutdown();
       delete m_dsp;
       m_dsp = NULL;
   }
}

PLUGIN_GLOBALS* CPluginManager::GetGlobals() {
    return m_PluginGlobals;
}

// ----------------------------------------
// Create list of available plugins
// -------------
void CPluginManager::ScanForPlugins()
{
	m_PluginInfos.clear();

	CFileSearch::XStringVector Directories;
	Directories.push_back(std::string(PLUGINS_DIR));

	CFileSearch::XStringVector Extensions;
        Extensions.push_back("*" PLUGIN_SUFFIX);

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if (rFilenames.size() > 0)
	{
		for (size_t i = 0; i < rFilenames.size(); i++)
		{
			std::string orig_name = rFilenames[i];
			std::string FileName;

			if (!SplitPath(rFilenames[i], NULL, &FileName, NULL))
			{
				printf("Bad Path %s\n", rFilenames[i].c_str());
				return;
			}

			CPluginInfo PluginInfo(orig_name.c_str());
			if (PluginInfo.IsValid())
			{
				m_PluginInfos.push_back(PluginInfo);
			}
		}
	}
}

Common::PluginPAD *CPluginManager::GetPAD(int controller)
{
    if (m_pad[controller] == NULL)
	m_pad[controller] = (Common::PluginPAD*)LoadPlugin(m_params.m_strPadPlugin[controller].c_str());

    return m_pad[controller];
}

Common::PluginWiimote *CPluginManager::GetWiimote(int controller)
{
    if (m_wiimote[controller] == NULL)
		m_wiimote[controller] = (Common::PluginWiimote*)LoadPlugin
			(m_params.m_strWiimotePlugin[controller].c_str());

    return m_wiimote[controller];
}

Common::PluginDSP *CPluginManager::GetDSP()
{
    if (m_dsp == NULL)
	m_dsp = (Common::PluginDSP*)LoadPlugin(m_params.m_strDSPPlugin.c_str());

    return m_dsp;
}

Common::PluginVideo *CPluginManager::GetVideo() {

    if (m_video == NULL)
	m_video = (Common::PluginVideo*)LoadPlugin(m_params.m_strVideoPlugin.c_str());

    return m_video;
}

void *CPluginManager::LoadPlugin(const char *_rFilename)//, PLUGIN_TYPE type)
{
    CPluginInfo info(_rFilename);
    PLUGIN_TYPE type = info.GetPluginInfo().Type;
    Common::CPlugin *plugin = NULL;
    switch (type) {
    case PLUGIN_TYPE_VIDEO:
	plugin = new Common::PluginVideo(_rFilename);
	break;

    case PLUGIN_TYPE_PAD:
	plugin = new Common::PluginPAD(_rFilename);
	break;

    case PLUGIN_TYPE_DSP:
	plugin = new Common::PluginDSP(_rFilename);
	break;

    case PLUGIN_TYPE_WIIMOTE:
	plugin = new Common::PluginWiimote(_rFilename);
	break;
    default:
	PanicAlert("Trying to load unsupported type %d", type);
    }

    if (!plugin->IsValid()) {
	PanicAlert("Can't open %s", _rFilename);
	return NULL;
    }
    
    plugin->SetGlobals(m_PluginGlobals);


    return plugin;
}

// ----------------------------------------
// Open config window. _rFilename = plugin filename ,ret = the dll slot number
// -------------
void CPluginManager::OpenConfig(void* _Parent, const char *_rFilename)
{

    Common::CPlugin *plugin = new Common::CPlugin(_rFilename);
    plugin->SetGlobals(m_PluginGlobals);
    plugin->Config((HWND)_Parent);
    delete plugin;
}

// ----------------------------------------
// Open debugging window. Type = Video or DSP. Show = Show or hide window.
// -------------
void CPluginManager::OpenDebug(void* _Parent, const char *_rFilename, PLUGIN_TYPE Type, bool Show)
{
    if (Type == PLUGIN_TYPE_VIDEO) {
	GetVideo()->Debug((HWND)_Parent, Show);
    } else if (Type == PLUGIN_TYPE_DSP) {
	GetDSP()->Debug((HWND)_Parent, Show);
    }
}

// ----------------------------------------
// Get dll info
// -------------
CPluginInfo::CPluginInfo(const char *_rFileName)
	: m_FileName(_rFileName)
	, m_Valid(false)
{
    Common::CPlugin *plugin = new Common::CPlugin(_rFileName);
    if (plugin->IsValid()) {
	if (plugin->GetInfo(m_PluginInfo))
	    m_Valid = true;
	else
	    PanicAlert("Could not get info about plugin %s", _rFileName);
	delete plugin;
    }
}


