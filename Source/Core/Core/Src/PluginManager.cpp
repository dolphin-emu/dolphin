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
// File description
/* ¯¯¯¯¯¯¯¯¯¯¯¯

   This file controls when plugins are loaded and unloaded from memory. Its functions scan for valid
   plugins when Dolphin is booted, and open the debugging and config windows. The PluginManager is
   created once when Dolphin starts and is closed when Dolphin is closed.

   When plugins are freed and loaded:

   In an attempt to avoid the crash that occurs when the use LoadLibrary() and FreeLibrary() often
   (every time a game is stopped and started) these functions will only be used when
		1. Dolphin is started
		2. A plugin is changed
		3. Dolphin is closed
   it will not be used when we Start and Stop games. With these exceptions:
		1. Video plugin: If FreeLibrary() is not called between Stop and Start it will fail for
		several games on the next Start, but not for all games.
		2. Sond plugin: If FreeLibrary() is not called between Stop and Start I got the "Tried to
		"get pointer for unknown address ffffffff" message for all games I tried.

	Currently this holds if the 'SETUP_FREE_PLUGIN_ON_BOOT' option is used

	For some reason the time when the FreeLibrary() is placed produce different results. If it's placed
	after ShutDown() I don't get a black screen when I start SSBM (PAL) again, if I have stopped the game
	before the first 3D appears (on the start screen), if I show the start screen and then Stop and Start
	I get the same error again (a black screen) (with the default OpenGL settings, copy EFB to texture, no
	hack). I also get the "Tried to get pointer ..." error then to (if DSP HLE sound has been enabled, if
	"Enable HLE Audio" has been disabled I don't get that error message). For this reason I have to place
	FreeVideo() and FreeDSP() before it's initialized on the next Start instead, then it works.

	If it was not for the crash I always got earlier after several (perhaps as many as twenty) Stop and Start
	I would be indifferent about how often FreeLibrary() i used, but since it seems like it can fail
	infrequently, at least for nJoy, I'd rather use FreeLibrary() more sparingly. However, I could not
	reproduce any crash now after several Stop and Start so maybe it has gone away or I was lucky. In any case
	if it works I'd rather be without FreeLibrary() between Start and Stop.

//////////////////////////////////////*/


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯
#include <string> // System
#include <vector>

#include "Common.h"
#include "PluginManager.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "Core.h"

#include "FileSearch.h" // Common
#include "FileUtil.h"
#include "StringUtil.h"
#include "ConsoleWindow.h"
#include "Setup.h"

// Create the plugin manager class
CPluginManager CPluginManager::m_Instance;
/////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// The Plugin Manager Class
// ¯¯¯¯¯¯¯¯¯¯¯¯

// The plugin manager is some sort of singleton that runs during Dolphin's entire lifespan.
CPluginManager::CPluginManager() : 
	m_params(SConfig::GetInstance().m_LocalCoreStartupParameter)
{
	m_PluginGlobals = new PLUGIN_GLOBALS;
	m_PluginGlobals->eventHandler = EventHandler::GetInstance();
	m_PluginGlobals->config = (void *)&SConfig::GetInstance();
	m_PluginGlobals->messageLogger = NULL;

	// Set initial values to NULL.
	m_video = NULL;
	m_dsp = NULL;
	for (int i = 0; i < MAXPADS; i++)
		m_pad[i] = NULL;
	for (int i = 0; i < MAXWIIMOTES; i++)
		m_wiimote[i] = NULL;
}

// This will call FreeLibrary() for all plugins
CPluginManager::~CPluginManager()
{
	Console::Print("Delete CPluginManager\n");

	delete m_PluginGlobals;
	delete m_dsp;

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
		if (m_wiimote[i])
			delete m_wiimote[i];

	delete m_video;
}
//////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Init and Shutdown Plugins 
// ¯¯¯¯¯¯¯¯¯¯¯¯
// Function: Point the m_pad[] and other variables to a certain plugin
bool CPluginManager::InitPlugins()
{
	if (!GetDSP()) {
		PanicAlert("Can't init DSP Plugin");
		return false;
	}
	Console::Print("Before GetVideo\n");

	if (!GetVideo()) {
		PanicAlert("Can't init Video Plugin");
		return false;
	}
	Console::Print("After GetVideo\n");

	// Check if we get at least one pad or wiimote
	bool pad = false;
	bool wiimote = false;

	// Init pad
	for (int i = 0; i < MAXPADS; i++)
	{
		// Check that the plugin has a name
		if (!m_params.m_strPadPlugin[i].empty())
			GetPad(i);
		// Check that GetPad succeeded
		if (m_pad[i] != NULL)
			pad = true;
	}
	if (!pad)
	{
		PanicAlert("Can't init any PAD Plugins");
		return false;
	}

	// Init wiimote
	if (m_params.bWii) {
		for (int i = 0; i < MAXWIIMOTES; i++) {
			if (!m_params.m_strWiimotePlugin[i].empty())
				GetWiimote(i);

			if (m_wiimote[i] != NULL)
				wiimote = true;
		}
		if (!wiimote) {
			PanicAlert("Can't init any Wiimote Plugins");
			return false;
		}
	}

	return true;
}


// FreeLibrary() after ShutDown() is disabled for some plugins. See the comment in the file description
// for an explanation about the current LoadLibrary() and FreeLibrary() behavior.
void CPluginManager::ShutdownPlugins()
{
	for (int i = 0; i < MAXPADS; i++) {
		if (m_pad[i])
		{
			m_pad[i]->Shutdown();
			//delete m_pad[i];
		}
		//m_pad[i] = NULL;
	}

	for (int i = 0; i < MAXWIIMOTES; i++)
	{
		if (m_wiimote[i])
		{
			m_wiimote[i]->Shutdown();
			//delete m_wiimote[i];
		}
		//m_wiimote[i] = NULL;
	}

	if (m_video)
	{
		m_video->Shutdown();
		// With this option, this is done on boot instead
		#ifndef SETUP_DONT_FREE_PLUGIN_ON_STOP
			delete m_video;
			m_video = NULL;
		#endif
	}


	if (m_dsp)
	{
		m_dsp->Shutdown();
		// With this option, this is done on boot instead
		#ifndef SETUP_DONT_FREE_PLUGIN_ON_STOP
			delete m_dsp;
			m_dsp = NULL;
		#endif
	}
}
//////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// The PluginInfo class: Find Valid Plugins
// ¯¯¯¯¯¯¯¯¯¯¯¯
/* Function: This info is used in ScanForPlugins() to check for valid plugins and and in LoadPlugin() to
   check that the filename we want to use is a good DLL. */
CPluginInfo::CPluginInfo(const char *_rFilename)
	: m_Filename(_rFilename)
	, m_Valid(false)
{
	if (!File::Exists(_rFilename))
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
}
///////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Supporting functions
// ¯¯¯¯¯¯¯¯¯¯¯¯

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
void *CPluginManager::LoadPlugin(const char *_rFilename, int Number)
{
	// Create a string of the filename
	std::string Filename = _rFilename;
	/* Avoid calling LoadLibrary() again and instead point to the plugin info that we found when
	   Dolphin was started */
	CPluginInfo *info = NULL;
	GetPluginInfo(info, Filename);
	if (info == NULL)
	{
		PanicAlert("Can't open %s, it's missing", _rFilename);
		return NULL;
	}

	PLUGIN_TYPE type = info->GetPluginInfo().Type;	
	Common::CPlugin *plugin = NULL;

	// Check again that the file exists, the first check is when CPluginInfo info is created
	if (!File::Exists(_rFilename))
		return NULL;
	
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
	
	// Check that the plugin has both all the common and all the type specific functions
	if (!plugin->IsValid())
	{
		PanicAlert("Can't open %s, it has a missing function", _rFilename);
		return NULL;
	}
	
	// Call the DLL function SetGlobals
	plugin->SetGlobals(m_PluginGlobals);
	return plugin;
}

/* Check if the plugin has already been initialized. If so, return the Id of
   the duplicate pad so we can point the new m_pad[] to that */
int CPluginManager::OkayToInitPlugin(int Plugin)
{
	// Compare it to the earlier plugins
	for (int i = 0; i < Plugin; i++)
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
	// Get plugins dir
	CFileSearch::XStringVector Directories;
	Directories.push_back(std::string(PLUGINS_DIR));
	
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
			std::string Filename;
			
			if (!SplitPath(rFilenames[i], NULL, &Filename, NULL)) {
				printf("Bad Path %s\n", rFilenames[i].c_str());
				return;
			}
			
			CPluginInfo PluginInfo(orig_name.c_str());
			if (PluginInfo.IsValid())
			{
				// Save the PluginInfo
				m_PluginInfos.push_back(PluginInfo);
			}
		}
	}
}
/////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
/* Create or return the already created plugin pointers. This will be called
   often for the Pad and Wiimote from the SI_.cpp files. And often for the DSP
   from the DSP files.
   
   We don't need to check if [Plugin]->IsValid() here because it will not be set by LoadPlugin()
   if it's not valid.
   */
// ¯¯¯¯¯¯¯¯¯¯¯¯
Common::PluginPAD *CPluginManager::GetPad(int controller)
{
	if (m_pad[controller] != NULL)
		if (m_pad[controller]->GetFilename() == m_params.m_strPadPlugin[controller])
			return m_pad[controller];
	
	// Else do this
	if (OkayToInitPlugin(controller) == -1) {
		m_pad[controller] = (Common::PluginPAD*)LoadPlugin(m_params.m_strPadPlugin[controller].c_str(), controller);
		Console::Print("LoadPlugin: %i\n", controller);
	}
	else {
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
	/* We don't need to check if m_video->IsValid() here, because m_video will not be set by LoadPlugin()
	   if it's not valid */
	if (m_video != NULL)
	{
		// Check if the video plugin has been changed
		if (m_video->GetFilename() == m_params.m_strVideoPlugin)
			return m_video;
		// Then free the current video plugin, 
		else
			FreeVideo();
	}

	// and load a new plugin
	m_video = (Common::PluginVideo*)LoadPlugin(m_params.m_strVideoPlugin.c_str());
	return m_video;
}

// Free plugins to completely reset all variables and potential DLLs loaded by
// the plugins in turn
void CPluginManager::FreeVideo()
{
	delete m_video;
	m_video = NULL;
}

void CPluginManager::FreeDSP()
{
	delete m_dsp;
	m_dsp = NULL;
}

void CPluginManager::FreePad(u32 Pad)
{
	if (Pad < MAXPADS) {
		delete m_pad[Pad];
		m_pad[Pad] = NULL;	
	}
}

void CPluginManager::FreeWiimote(u32 Wiimote)
{
	if (Wiimote < MAXWIIMOTES)
	{
		delete m_wiimote[Wiimote];
		m_wiimote[Wiimote] = NULL;	
	}
}
///////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// Call DLL functions
// ¯¯¯¯¯¯¯¯¯¯¯¯

// Open config window. Input: _rFilename = Plugin filename , Type = Plugin type
void CPluginManager::OpenConfig(void* _Parent, const char *_rFilename, PLUGIN_TYPE Type)
{
	if (! File::Exists(_rFilename)) {
		PanicAlert("Can't find plugin %s", _rFilename);
		return;
	}
	
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
	default:
		PanicAlert("Type %d config not supported in plugin %s", Type, _rFilename); 
	}
}

// Open debugging window. Type = Video or DSP. Show = Show or hide window.
void CPluginManager::OpenDebug(void* _Parent, const char *_rFilename, PLUGIN_TYPE Type, bool Show)
{
	if (!File::Exists(_rFilename))
	{
		PanicAlert("Can't find plugin %s", _rFilename);
		return;
	}

	switch(Type) 
	{
	case PLUGIN_TYPE_VIDEO:
		GetVideo()->Debug((HWND)_Parent, Show);
		break;
	case PLUGIN_TYPE_DSP:
		GetDSP()->Debug((HWND)_Parent, Show);
		break;
	default:
		PanicAlert("Type %d debug not supported in plugin %s", Type, _rFilename); 
	}
}
///////////////////////////////////////////
