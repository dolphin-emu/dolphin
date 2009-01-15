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
#include "StringUtil.h"


CPluginManager CPluginManager::m_Instance;


CPluginManager::CPluginManager()
{
    m_PluginGlobals = new PLUGIN_GLOBALS;
    m_PluginGlobals->eventHandler = EventHandler::GetInstance();
    m_PluginGlobals->config = NULL;
    m_PluginGlobals->messageLogger = NULL;
}


CPluginManager::~CPluginManager()
{
    if (m_PluginGlobals)
	delete m_PluginGlobals;

    if (m_dsp)
	delete m_dsp;

    if (m_video)
	delete m_video;
    
    for (int i=0;i<1;i++) {
	if (m_pad[i])
	    delete m_pad[i];

	if (m_wiimote[i])
	    delete m_wiimote[i];
}
}

bool CPluginManager::InitPlugins(SCoreStartupParameter scsp) {

    // TODO error checking
    m_dsp = (Common::PluginDSP*)LoadPlugin(scsp.m_strDSPPlugin.c_str());
    if (!m_dsp) {
	return false;
    }

    m_video = (Common::PluginVideo*)LoadPlugin(scsp.m_strVideoPlugin.c_str());
    if (!m_video)
	return false;

    for (int i=0;i<1;i++) {
	m_pad[i] = (Common::PluginPAD*)LoadPlugin(scsp.m_strPadPlugin.c_str());
	if (m_pad[i] == NULL)
	    return false;

	if (scsp.bWii) {
	    m_wiimote[i] = (Common::PluginWiimote*)LoadPlugin
		(scsp.m_strWiimotePlugin.c_str());
	    if (m_wiimote[i] == NULL)
		return false;
	}
    }

    return true;
}

void CPluginManager::ShutdownPlugins() {
   for (int i=0;i<1;i++) {
       if (m_pad[i])
	   m_pad[i]->Shutdown();
       if (m_wiimote[i])
	   m_wiimote[i]->Shutdown();
   }

   if (m_video)
       m_video->Shutdown();

   if (m_dsp)
       m_dsp->Shutdown();
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

Common::PluginPAD *CPluginManager::GetPAD(int controller) {
    //    if (m_pad[controller] == NULL)
    //	InitPlugins();

    return m_pad[controller];
}

Common::PluginWiimote *CPluginManager::GetWiimote(int controller) {
    //    if (m_wiimote[controller] == NULL)
    //	InitPlugins();
    
    return m_wiimote[controller];
}

Common::PluginDSP *CPluginManager::GetDSP() {
    //    if (m_dsp == NULL)
    //	InitPlugins();
    
    return m_dsp;
}

Common::PluginVideo *CPluginManager::GetVideo() {
    //    if (m_video == NULL)
    //	InitPlugins();

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
	//int ret = 1;
	//int ret = Common::CPlugin::Load(_rFilename, true);
	//int ret = PluginVideo::LoadPlugin(_rFilename);
	//int ret = PluginDSP::LoadPlugin(_rFilename);

    
    if (Type == PLUGIN_TYPE_VIDEO) {
	if(!m_video)
	    m_video = (Common::PluginVideo*)LoadPlugin(_rFilename);
	m_video->Debug((HWND)_Parent, Show);
    } else if (Type == PLUGIN_TYPE_DSP) {
	if (!m_dsp)
	    m_dsp = (Common::PluginDSP*)LoadPlugin(_rFilename);
	m_dsp->Debug((HWND)_Parent, Show);
    }
    /*		if (Type)
		{
			//Common::CPlugin::Debug((HWND)_Parent);
			if (!PluginVideo::IsLoaded())
				PluginVideo::LoadPlugin(_rFilename);

			//PluginVideo::SetDllGlobals(m_PluginGlobals);
			PluginVideo::Debug((HWND)_Parent, Show);
		}
		else
		{
			if(!PluginDSP::IsLoaded())
			    PluginDSP::LoadPlugin(_rFilename);

			//PluginDSP::SetDllGlobals(m_PluginGlobals);
			PluginDSP::Debug((HWND)_Parent, Show);
			}*/
		//Common::CPlugin::Release(); // this is only if the wx dialog is called with ShowModal()

		//m_DllDebugger = (void (__cdecl*)(HWND))PluginVideo::plugin.Get("DllDebugger");
		//m_DllDebugger(NULL);
}

// ----------------------------------------
// Get dll info
// -------------
CPluginInfo::CPluginInfo(const char *_rFileName)
	: m_FileName(_rFileName)
	, m_Valid(false)
{
    Common::CPlugin *plugin = new Common::CPlugin(_rFileName);
    if (plugin->IsValid())
	{
	    if (plugin->GetInfo(m_PluginInfo))
			m_Valid = true;
		else
			PanicAlert("Could not get info about plugin %s", _rFileName);

	    delete plugin;
	}
	/*
	The DLL loading code provides enough error messages already. Possibly make some return codes
	and handle messages here instead?
	else
	{
		if (!File::Exists(_rFileName)) {
			PanicAlert("Could not load plugin %s - file does not exist", _rFileName);
		} else {
			PanicAlert("Failed to load plugin %s - unknown error.\n", _rFileName);
		}
	}*/
}


