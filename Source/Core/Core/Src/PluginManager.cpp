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


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯
#include <string> // System
#include <vector>

//#include "Globals.h" // Local
#include "PluginManager.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "Core.h"

#include "FileSearch.h" // Common
#include "FileUtil.h"
#include "StringUtil.h"
#include "ConsoleWindow.h"

CPluginManager CPluginManager::m_Instance;

//#define INPUTCOMMON
//////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// The Plugin Manager Class
// ¯¯¯¯¯¯¯¯¯¯¯¯
CPluginManager::CPluginManager() : 
    m_params(SConfig::GetInstance().m_LocalCoreStartupParameter)
{
    m_PluginGlobals = new PLUGIN_GLOBALS;
	

    m_PluginGlobals->eventHandler = EventHandler::GetInstance();
    m_PluginGlobals->config = (void *)&SConfig::GetInstance();
    m_PluginGlobals->messageLogger = NULL;

	#ifdef INPUTCOMMON
		m_InputManager = new InputManager();
		m_PluginGlobals->inputManager = m_InputManager;
	#endif
}

/* Function: FreeLibrary()
   Called from: In an attempt to avoid the crash that occurs when the use LoadLibrary() and
   FreeLibrary() often (every game a game is stopped and started) these functions will only
   be used when
		1. Dolphin is started
		2. A plugin is changed
		3. Dolphin is closed
   it will not be used when we Start and Stop games. */
CPluginManager::~CPluginManager()
{
	Console::Print("Delete CPluginManager\n");

    if (m_PluginGlobals) delete m_PluginGlobals;

    if (m_dsp) delete m_dsp;

    if (m_video) delete m_video;
    
	/**/
    for (int i = 0; i < MAXPADS; i++)
	{
		if (m_pad[i] && OkayToInitPlugin(i))
		{
			Console::Print("Delete: %i\n", i);
			delete m_pad[i];
		}
		m_pad[i] = NULL;
	}
	
    
    for (int i = 0; i < MAXWIIMOTES; i++)
		if (m_wiimote[i]) delete m_wiimote[i];
}
//////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Init and ShutDown Plugins 
// ¯¯¯¯¯¯¯¯¯¯¯¯

// Point the m_pad[] and other variables to a certain plugin
bool CPluginManager::InitPlugins()
{
    if (! GetDSP())
	{
		PanicAlert("Can't init DSP Plugin");
		return false;
    }

	if (! GetVideo())
	{
		PanicAlert("Can't init Video Plugin");
		return false;
	}

	// Check if we get at least one pad or wiimote
    bool pad = false;
    bool wiimote = false;

	// Init pad
    for (int i = 0; i < MAXPADS; i++)
	{
		if (! m_params.m_strPadPlugin[i].empty())
			GetPad(i);
		if (m_pad[i] != NULL)
			pad = true;
    }
    if (! pad)
	{
		PanicAlert("Can't init any PAD Plugins");
		return false;
    }

	// Init wiimote
    if (m_params.bWii)
	{
		for (int i = 0; i < MAXWIIMOTES; i++)
		{
			if (! m_params.m_strWiimotePlugin[i].empty())
				GetWiimote(i);

			if (m_wiimote[i] != NULL)
				wiimote = true;
		}
		if (! wiimote)
		{
			PanicAlert("Can't init any Wiimote Plugins");
			return false;
		}
    }

    return true;
}

void CPluginManager::ShutdownPlugins()
{
	// Check if we can shutdown the plugin
	for (int i = 0; i < MAXPADS; i++)
	{
		if (m_pad[i] && OkayToInitPlugin(i))
		{
			//Console::Print("Shutdown: %i\n", i);
			m_pad[i]->Shutdown();
			//delete m_pad[i];
		}		
		//m_pad[i] = NULL;
	}

	for (int i = 0; i < MAXWIIMOTES; i++)
		if (m_wiimote[i]) m_wiimote[i]->Shutdown();

	if (m_video)
		m_video->Shutdown();

	if (m_dsp)
		m_dsp->Shutdown();
}
//////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Supporting functions
// ¯¯¯¯¯¯¯¯¯¯¯¯
/* Called from: Get__() functions in this file only (not from anywhere else), therefore we
   can leave all condition checks in the Get__() functions below. */
void *CPluginManager::LoadPlugin(const char *_rFilename, int Number)//, PLUGIN_TYPE type)
{
    CPluginInfo info(_rFilename);
    PLUGIN_TYPE type = info.GetPluginInfo().Type;
	//std::string Filename = info.GetPluginInfo().Filename;
	std::string Filename = _rFilename;
	Common::CPlugin *plugin = NULL;

    switch (type)
	{
    case PLUGIN_TYPE_VIDEO:
		plugin = new Common::PluginVideo(_rFilename);
		break;

    case PLUGIN_TYPE_DSP:
		plugin = new Common::PluginDSP(_rFilename);
		break;

	case PLUGIN_TYPE_PAD:
		plugin = new Common::PluginPAD(_rFilename);
		break;

    case PLUGIN_TYPE_WIIMOTE:
		plugin = new Common::PluginWiimote(_rFilename);
		break;

	default:
		PanicAlert("Trying to load unsupported type %d", type);
    }

    if (!plugin->IsValid())
	{
		PanicAlert("Can't open %s", _rFilename);
		return NULL;
    }
    
    plugin->SetGlobals(m_PluginGlobals);
    return plugin;
}

// ----------------------------------------
/* Check if the plugin has already been initialized. If so, return the Id of the duplicate pad
   so we can point the new m_pad[] to that */
// -------------
int CPluginManager::OkayToInitPlugin(int Plugin)
{
	// Compare it to the earlier plugins
	for(int i = 0; i < Plugin; i++)
		if (m_params.m_strPadPlugin[Plugin] == m_params.m_strPadPlugin[i])
			return i;

	// No there is no duplicate plugin
	return -1;	
}


PLUGIN_GLOBALS* CPluginManager::GetGlobals()
{
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
/////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
/* Create or return the already created plugin pointers. This will be called often for the
   Pad and Wiimote from the SI_.cpp files. And often for the DSP from the DSP files. */
// ¯¯¯¯¯¯¯¯¯¯¯¯
Common::PluginPAD *CPluginManager::GetPad(int controller)
{
	if (m_pad[controller] != NULL)
		if (m_pad[controller]->GetFilename() == m_params.m_strPadPlugin[controller])
			return m_pad[controller];
		
	// Else do this
	if(OkayToInitPlugin(controller) == -1)
	{
		m_pad[controller] = (Common::PluginPAD*)LoadPlugin(m_params.m_strPadPlugin[controller].c_str(), controller);
		Console::Print("LoadPlugin: %i\n", controller);
	}
	else
	{
		Console::Print("Pointed: %i to %i\n", controller, OkayToInitPlugin(controller));
		m_pad[controller] = m_pad[OkayToInitPlugin(controller)];
	}	
	return m_pad[controller];
}

Common::PluginWiimote *CPluginManager::GetWiimote(int controller)
{
	if (m_wiimote[controller] != NULL)
		if (m_wiimote[controller]->GetFilename() == m_params.m_strWiimotePlugin[controller])
			return m_wiimote[controller];

	// Else load a new plugin
	m_wiimote[controller] = (Common::PluginWiimote*)LoadPlugin(m_params.m_strWiimotePlugin[controller].c_str());
    return m_wiimote[controller];
}

Common::PluginDSP *CPluginManager::GetDSP()
{
	if (m_dsp != NULL)
		if (m_dsp->GetFilename() == m_params.m_strDSPPlugin)
			return m_dsp;
	// Else load a new plugin
	m_dsp = (Common::PluginDSP*)LoadPlugin(m_params.m_strDSPPlugin.c_str());
    return m_dsp;
}

Common::PluginVideo *CPluginManager::GetVideo()
{
	if (m_video != NULL)
		if (m_video->GetFilename() == m_params.m_strVideoPlugin)
			return m_video;

	// Else load a new plugin
	m_video = (Common::PluginVideo*)LoadPlugin(m_params.m_strVideoPlugin.c_str());
    return m_video;
}

// ----------------------------------------
// Free plugins to completely reset all variables and potential DLLs loaded by the plugins in turn
// -------------
Common::PluginVideo *CPluginManager::FreeVideo()
{
	if(m_video)
		delete m_video;
	m_video = NULL;
	m_video = (Common::PluginVideo*)LoadPlugin(m_params.m_strVideoPlugin.c_str(), 0);
	return m_video;
}
Common::PluginPAD *CPluginManager::FreePad()
{
	delete m_pad[0];
	m_pad[0] = NULL; m_pad[1] = NULL; m_pad[2] = NULL; m_pad[3] = NULL;
	m_pad[0] = (Common::PluginPAD*)LoadPlugin(m_params.m_strPadPlugin[0].c_str(), 0);
	return m_pad[0];

}
///////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Call DLL functions
// ¯¯¯¯¯¯¯¯¯¯¯¯

// ----------------------------------------
// Open config window. Input: _rFilename = Plugin filename , Type = Plugin type
// -------------
void CPluginManager::OpenConfig(void* _Parent, const char *_rFilename, PLUGIN_TYPE Type)
{
	#ifdef INPUTCOMMON
		m_InputManager->Init();
	#endif

	switch(Type)
	{
	case PLUGIN_TYPE_VIDEO:
		GetVideo()->Config((HWND)_Parent);
		break;
	case PLUGIN_TYPE_DSP:
		GetDSP()->Config((HWND)_Parent);
		break;
	case PLUGIN_TYPE_PAD:
		GetPad(0)->Config((HWND)_Parent);
		break;
	case PLUGIN_TYPE_WIIMOTE:
		GetWiimote(0)->Config((HWND)_Parent);
		break;
    }

	#ifdef INPUTCOMMON
		m_InputManager->Shutdown();
	#endif
}

// ----------------------------------------
// Open debugging window. Type = Video or DSP. Show = Show or hide window.
// -------------
void CPluginManager::OpenDebug(void* _Parent, const char *_rFilename, PLUGIN_TYPE Type, bool Show)
{
	switch(Type)
	{
	case PLUGIN_TYPE_VIDEO:
		GetVideo()->Debug((HWND)_Parent, Show);
		break;
	case PLUGIN_TYPE_DSP:
		GetDSP()->Debug((HWND)_Parent, Show);
		break;
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
    if (plugin->IsValid())
	{
	    if (plugin->GetInfo(m_PluginInfo))
			m_Valid = true;
		else
			PanicAlert("Could not get info about plugin %s", _rFileName);

	    delete plugin;
	}
}
///////////////////////////////////////////

