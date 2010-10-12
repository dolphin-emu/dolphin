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

// File description
/* ------------
   This file controls when plugins are loaded and unloaded from memory. Its functions scan for valid
   plugins when Dolphin is booted, and open the debugging and config windows. The PluginManager is
   created once when Dolphin starts and is closed when Dolphin is closed.
*/

// Include
// ------------
#include <string> // System
#include <vector>

#include "Common.h"
#include "CommonPaths.h"
#include "PluginManager.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "Core.h"
#include "Host.h"

#include "FileSearch.h" // Common
#include "FileUtil.h"
#include "StringUtil.h"
#include "MemoryUtil.h"
#include "Setup.h"

// Create the plugin manager class
CPluginManager* CPluginManager::m_Instance;

// The Plugin Manager Class
// ------------

void CPluginManager::Init()
{
	m_Instance = new CPluginManager;
}

void CPluginManager::Shutdown()
{
	delete m_Instance;
	m_Instance = NULL;
}

// The plugin manager is some sort of singleton that runs during Dolphin's entire lifespan.
CPluginManager::CPluginManager()
{
	m_PluginGlobals = new PLUGIN_GLOBALS;

	// Start LogManager
	m_PluginGlobals->logManager = LogManager::GetInstance();

	m_params = &(SConfig::GetInstance().m_LocalCoreStartupParameter);

	// Set initial values to NULL.
	m_video = NULL;
	m_dsp = NULL;
}

// This will call FreeLibrary() for all plugins
CPluginManager::~CPluginManager()
{
	INFO_LOG(CONSOLE, "Delete CPluginManager\n");

	delete m_PluginGlobals;
	delete m_dsp;
	delete m_video;
}


// Init and Shutdown Plugins 
// ------------
// Function: Point the m_pad[] and other variables to a certain plugin
bool CPluginManager::InitPlugins()
{
	// Update pluginglobals.
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIni.size() == 0)
	{
		PanicAlert("Bad gameini filename");
	}
	strcpy(m_PluginGlobals->game_ini, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIni.c_str());
	strcpy(m_PluginGlobals->unique_id, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str());
	INFO_LOG(CONSOLE, "Before GetVideo\n");

	if (!GetVideo()) {
		PanicAlert("Can't init Video Plugin");
		return false;
	}
	INFO_LOG(CONSOLE, "After GetVideo\n");

	if (!GetDSP()) {
		PanicAlert("Can't init DSP Plugin");
		return false;
	}

	return true;
}


// FreeLibrary() after ShutDown() is disabled for some plugins. See the comment in the file description
// for an explanation about the current LoadLibrary() and FreeLibrary() behavior.
void CPluginManager::ShutdownPlugins()
{
	if (m_dsp)
	{
		m_dsp->Shutdown();
		FreeDSP();
		NOTICE_LOG(CONSOLE, "%s", Core::StopMessage(false, "Audio shutdown").c_str());
	}
}

void CPluginManager::ShutdownVideoPlugin()
{
	if (m_video)
	{
		m_video->Shutdown();
		FreeVideo();
		NOTICE_LOG(CONSOLE, "%s", Core::StopMessage(false, "Video shutdown").c_str());
	}
}



// The PluginInfo class: Find Valid Plugins
// ------------
/* Function: This info is used in ScanForPlugins() to check for valid plugins and and in LoadPlugin() to
   check that the filename we want to use is a good DLL. */
CPluginInfo::CPluginInfo(const char *_rFilename)
	: m_Filename(_rFilename)
	, m_Valid(false)
{
	if (!File::Exists((File::GetPluginsDirectory() + _rFilename).c_str()))
	{
		PanicAlert("Can't find plugin %s", _rFilename);
		return;
	}

	// Check if the functions that are common to all plugins are present
	Common::CPlugin *plugin = new Common::CPlugin(_rFilename);
	if (plugin->IsValid())
	{
		if (plugin->GetInfo(m_PluginInfo))
			m_Valid = true;
		else
			PanicAlert("Could not get info about plugin %s", _rFilename);
		// We are now done with this plugin and will call FreeLibrary()
		delete plugin;
	}
	else
	{
		WARN_LOG(CONSOLE, "PluginInfo: %s is not a valid Dolphin plugin. Ignoring.", _rFilename);
	}
}




// Supporting functions
// ------------

/* Return the plugin info we saved when Dolphin was started. We don't even add a function to try load a
   plugin name that was not found because in that case it must have been deleted while Dolphin was running.
   If the user has done that he will instead get the "Can't open %s, it's missing" message. */
void CPluginManager::GetPluginInfo(CPluginInfo *&info, std::string Filename)
{
	for (int i = 0; i < (int)m_PluginInfos.size(); i++)
	{
		if (m_PluginInfos.at(i).GetFilename() == Filename)
		{
			info = &m_PluginInfos.at(i);
			return;
		}
	}
}

/* Called from: Get__() functions in this file only (not from anywhere else),
   therefore we can leave all condition checks in the Get__() functions
   below. */
void *CPluginManager::LoadPlugin(const char *_rFilename)
{
	if (!File::Exists((File::GetPluginsDirectory() + _rFilename).c_str())) {
		PanicAlert("Error loading plugin %s: can't find file. Please re-select your plugins.", _rFilename);
		return NULL;
	}
	/* Avoid calling LoadLibrary() again and instead point to the plugin info that we found when
	   Dolphin was started */
	CPluginInfo *info = NULL;
	GetPluginInfo(info, _rFilename);
	if (!info) {
		PanicAlert("Error loading %s: can't read info", _rFilename);
		return NULL;
	}
	
	PLUGIN_TYPE type = info->GetPluginInfo().Type;
	Common::CPlugin *plugin = NULL;

	switch (type)
	{
	case PLUGIN_TYPE_VIDEO:
		plugin = new Common::PluginVideo(_rFilename);
		break;
	
	case PLUGIN_TYPE_DSP:
		plugin = new Common::PluginDSP(_rFilename);
		break;
	
	default:
		PanicAlert("Trying to load unsupported type %d", type);
		return NULL;
	}
	
	// Check that the plugin has all the common and all the type specific functions
	if (!plugin->IsValid())
	{
		PanicAlert("Can't open %s, it has a missing function", _rFilename);
		delete plugin;
		return NULL;
	}
	
	// Call the DLL function SetGlobals
	plugin->SetGlobals(m_PluginGlobals);
	return plugin;
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
	// Get plugins dir
	CFileSearch::XStringVector Directories;

	Directories.push_back(File::GetPluginsDirectory());

	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*" PLUGIN_SUFFIX);
	// Get all DLL files in the plugins dir
	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if (rFilenames.size() > 0)
	{
		for (size_t i = 0; i < rFilenames.size(); i++)
		{
			std::string orig_name = rFilenames[i];
			std::string Filename, Extension;
			
			if (!SplitPath(rFilenames[i], NULL, &Filename, &Extension)) {
				printf("Bad Path %s\n", rFilenames[i].c_str());
				return;
			}

			// Leave off the directory component
			std::string StoredName = Filename;
			StoredName += Extension;

			CPluginInfo PluginInfo(StoredName.c_str());
			if (PluginInfo.IsValid())
			{
				// Save the Plugin
				m_PluginInfos.push_back(PluginInfo);
			}
		}
	}
}


/* Create or return the already created plugin pointers. This will be called
   often for the DSP from the DSP files.
   
   We don't need to check if [Plugin]->IsValid() here because it will not be set by LoadPlugin()
   if it's not valid.
   */

Common::PluginDSP *CPluginManager::GetDSP()
{
	if (m_dsp != NULL)
	{
		if (m_dsp->GetFilename() == m_params->m_strDSPPlugin)
			return m_dsp;
		else
			FreeDSP();
	}
	// Else load a new plugin
	m_dsp = (Common::PluginDSP*)LoadPlugin(m_params->m_strDSPPlugin.c_str());
	return m_dsp;
}

Common::PluginVideo *CPluginManager::GetVideo()
{
	/* We don't need to check if m_video->IsValid() here, because m_video will not be set by LoadPlugin()
	   if it's not valid */
	if (m_video != NULL)
	{
		return m_video;
	}

	// and load a new plugin
	m_video = (Common::PluginVideo*)LoadPlugin(m_params->m_strVideoPlugin.c_str());
	return m_video;
}

// Free plugins to completely reset all variables and potential DLLs loaded by
// the plugins in turn
void CPluginManager::FreeVideo()
{
	WARN_LOG(CONSOLE, "%s", Core::StopMessage(false, "Will unload video DLL").c_str());
	delete m_video;
	m_video = NULL;
}

void CPluginManager::FreeDSP()
{
	WARN_LOG(CONSOLE, "%s", Core::StopMessage(false, "Will unload audio DLL").c_str());
	delete m_dsp;
	m_dsp = NULL;
}

void CPluginManager::EmuStateChange(PLUGIN_EMUSTATE newState)
{
	GetVideo()->EmuStateChange(newState);
	GetDSP()->EmuStateChange(newState);
}



// Call DLL functions
// ------------

// Open config window. Input: _rFilename = Plugin filename , Type = Plugin type
void CPluginManager::OpenConfig(void* _Parent, const char *_rFilename, PLUGIN_TYPE Type)
{
	if (! File::Exists((File::GetPluginsDirectory() + _rFilename).c_str()))
	{
		PanicAlert("Can't find plugin %s", _rFilename);
		return;
	}
	
	switch(Type)
	{
	case PLUGIN_TYPE_VIDEO:
		if (GetVideo() != NULL)
			GetVideo()->Config(_Parent);
		break;

	case PLUGIN_TYPE_DSP:
		if (GetDSP() != NULL)
			GetDSP()->Config(_Parent);
		break;

	default:
		PanicAlert("Type %d config not supported in plugin %s", Type, _rFilename);
		break;
	}
}

// Open debugging window. Type = Video or DSP. Show = Show or hide window.
void *CPluginManager::OpenDebug(void* _Parent, const char *_rFilename, PLUGIN_TYPE Type, bool Show)
{
	if (! File::Exists((File::GetPluginsDirectory() + _rFilename).c_str()))
	{
		PanicAlert("Can't find plugin %s", _rFilename);
		return NULL;
	}

	switch(Type)
	{
	case PLUGIN_TYPE_VIDEO:
		return GetVideo()->Debug(_Parent, Show);
		break;

	case PLUGIN_TYPE_DSP:
		return GetDSP()->Debug(_Parent, Show);
		break;

	default:
		PanicAlert("Type %d debug not supported in plugin %s", Type, _rFilename);
		return NULL;
		break;
	}
}
