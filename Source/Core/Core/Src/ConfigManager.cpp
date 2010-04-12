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

#include <string>

#include "Common.h"
#include "IniFile.h"
#include "ConfigManager.h"
#include "PluginManager.h"
#include "FileUtil.h"

SConfig* SConfig::m_Instance;

SConfig::SConfig()
{
	// Make sure we have log manager
	LoadSettings();
	//Make sure we load settings
	LoadSettingsHLE();
	LoadSettingsWii();

}

void SConfig::Init()
{
	m_Instance = new SConfig;
}

void SConfig::Shutdown()
{
	delete m_Instance;
	m_Instance = NULL;
}

SConfig::~SConfig()
{
	SaveSettings();
	delete m_SYSCONF;
}


void SConfig::SaveSettings()
{
	NOTICE_LOG(BOOT, "Saving Settings to %s", File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX)); // load first to not kill unknown stuff

	// General
	ini.Set("General", "LastFilename",	m_LastFilename);

	// ISO folders
	ini.Set("General", "GCMPathes",		(int)m_ISOFolder.size());

	for (size_t i = 0; i < m_ISOFolder.size(); i++)
	{
		TCHAR tmp[16];
		sprintf(tmp, "GCMPath%i", (int)i);
		ini.Set("General", tmp, m_ISOFolder[i]);
	}

	ini.Set("General", "RecersiveGCMPaths", m_RecursiveISOFolder);

	// Interface		
	ini.Set("Interface", "ConfirmStop",			m_LocalCoreStartupParameter.bConfirmStop);
	ini.Set("Interface", "UsePanicHandlers",	m_LocalCoreStartupParameter.bUsePanicHandlers);
	ini.Set("Interface", "HideCursor",			m_LocalCoreStartupParameter.bHideCursor);
	ini.Set("Interface", "AutoHideCursor",		m_LocalCoreStartupParameter.bAutoHideCursor);
	ini.Set("Interface", "Theme",				m_LocalCoreStartupParameter.iTheme);
	ini.Set("Interface", "MainWindowPosX",		m_LocalCoreStartupParameter.iPosX);
	ini.Set("Interface", "MainWindowPosY",		m_LocalCoreStartupParameter.iPosY);
	ini.Set("Interface", "MainWindowWidth",		m_LocalCoreStartupParameter.iWidth);
	ini.Set("Interface", "MainWindowHeight",	m_LocalCoreStartupParameter.iHeight);
	ini.Set("Interface", "Language",			m_InterfaceLanguage);
	ini.Set("Interface", "ShowToolbar",			m_InterfaceToolbar);
	ini.Set("Interface", "ShowStatusbar",		m_InterfaceStatusbar);
	ini.Set("Interface", "ShowLogWindow",		m_InterfaceLogWindow);
	ini.Set("Interface", "ShowConsole",			m_InterfaceConsole);

	// Hotkeys
	ini.Set("Hotkeys", "ToggleFullscreen", 			m_LocalCoreStartupParameter.iHotkey[HK_FULLSCREEN]);
	ini.Set("Hotkeys", "PlayPause", 				m_LocalCoreStartupParameter.iHotkey[HK_PLAY_PAUSE]);
	ini.Set("Hotkeys", "Stop",						m_LocalCoreStartupParameter.iHotkey[HK_STOP]);
	ini.Set("Hotkeys", "ToggleFullscreenModifier",	m_LocalCoreStartupParameter.iHotkeyModifier[HK_FULLSCREEN]);
	ini.Set("Hotkeys", "PlayPauseModifier", 		m_LocalCoreStartupParameter.iHotkeyModifier[HK_PLAY_PAUSE]);
	ini.Set("Hotkeys", "StopModifier",  			m_LocalCoreStartupParameter.iHotkeyModifier[HK_STOP]);

	// Display
	ini.Set("Display", "Fullscreen",			m_LocalCoreStartupParameter.bFullscreen);
	ini.Set("Display", "RenderToMain",			m_LocalCoreStartupParameter.bRenderToMain);
	ini.Set("Display", "RenderWindowXPos",		m_LocalCoreStartupParameter.iRenderWindowXPos);
	ini.Set("Display", "RenderWindowYPos",		m_LocalCoreStartupParameter.iRenderWindowYPos);
	ini.Set("Display", "RenderWindowWidth",		m_LocalCoreStartupParameter.iRenderWindowWidth);
	ini.Set("Display", "RenderWindowHeight",	m_LocalCoreStartupParameter.iRenderWindowHeight);

	// Game List Control
	ini.Set("GameList", "ListDrives",	m_ListDrives);
	ini.Set("GameList", "ListWad",		m_ListWad);
	ini.Set("GameList", "ListWii",		m_ListWii);
	ini.Set("GameList", "ListGC",		m_ListGC);
	ini.Set("GameList", "ListJap",		m_ListJap);
	ini.Set("GameList", "ListPal",		m_ListPal);
	ini.Set("GameList", "ListUsa",		m_ListUsa);
	ini.Set("GameList", "ListFrance",	m_ListFrance);
	ini.Set("GameList", "ListItaly",	m_ListItaly);
	ini.Set("GameList", "ListKorea",	m_ListKorea);
	ini.Set("GameList", "ListTaiwan",	m_ListTaiwan);
	ini.Set("GameList", "ListUnknown",	m_ListUnknown);

	// Core
	ini.Set("Core", "HLE_BS2",			m_LocalCoreStartupParameter.bHLE_BS2);
	ini.Set("Core", "CPUCore",			m_LocalCoreStartupParameter.iCPUCore);
	ini.Set("Core", "CPUThread",		m_LocalCoreStartupParameter.bCPUThread);
	ini.Set("Core", "DSPThread",		m_LocalCoreStartupParameter.bDSPThread);
	ini.Set("Core", "SkipIdle",			m_LocalCoreStartupParameter.bSkipIdle);
	ini.Set("Core", "LockThreads",		m_LocalCoreStartupParameter.bLockThreads);
	ini.Set("Core", "DefaultGCM",		m_LocalCoreStartupParameter.m_strDefaultGCM);
	ini.Set("Core", "DVDRoot",			m_LocalCoreStartupParameter.m_strDVDRoot);
	ini.Set("Core", "Apploader",		m_LocalCoreStartupParameter.m_strApploader);
	ini.Set("Core", "EnableCheats",		m_LocalCoreStartupParameter.bEnableCheats);
	ini.Set("Core", "SelectedLanguage",	m_LocalCoreStartupParameter.SelectedLanguage);
	ini.Set("Core", "MemcardA",			m_strMemoryCardA);
	ini.Set("Core", "MemcardB",			m_strMemoryCardB);
	ini.Set("Core", "SlotA",			m_EXIDevice[0]);
	ini.Set("Core", "SlotB",			m_EXIDevice[1]);
	ini.Set("Core", "SerialPort1",		m_EXIDevice[2]);
	char sidevicenum[16];
	for (int i = 0; i < 4; ++i)
	{
		sprintf(sidevicenum, "SIDevice%i", i);
		ini.Set("Core", sidevicenum, m_SIDevice[i]);
	}

	ini.Set("Core", "WiiSDCard", m_WiiSDCard);
	ini.Set("Core", "WiiKeyboard", m_WiiKeyboard);
	ini.Set("Core", "RunCompareServer",	m_LocalCoreStartupParameter.bRunCompareServer);
	ini.Set("Core", "RunCompareClient",	m_LocalCoreStartupParameter.bRunCompareClient);
	ini.Set("Core", "FrameLimit",		m_Framelimit);

	// Plugins
	ini.Set("Core", "GFXPlugin",	m_LocalCoreStartupParameter.m_strVideoPlugin);
	ini.Set("Core", "DSPPlugin",	m_LocalCoreStartupParameter.m_strDSPPlugin);
	ini.Set("Core", "PadPlugin",	m_LocalCoreStartupParameter.m_strPadPlugin[0]);
	ini.Set("Core", "WiiMotePlugin",m_LocalCoreStartupParameter.m_strWiimotePlugin[0]);

	ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	m_SYSCONF->Save();
}


void SConfig::LoadSettings()
{
	INFO_LOG(BOOT, "Loading Settings from %s", File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));

	std::string PluginsDir = File::GetPluginsDirectory();
	
	// Hard coded default
	m_DefaultGFXPlugin = PluginsDir + DEFAULT_GFX_PLUGIN;
	m_DefaultDSPPlugin = PluginsDir + DEFAULT_DSP_PLUGIN;
	m_DefaultPADPlugin = PluginsDir + DEFAULT_PAD_PLUGIN;
	m_DefaultWiiMotePlugin = PluginsDir + DEFAULT_WIIMOTE_PLUGIN;

	// General
	{
		ini.Get("General", "LastFilename",	&m_LastFilename);

		m_ISOFolder.clear();
		int numGCMPaths;

		if (ini.Get("General", "GCMPathes", &numGCMPaths, 0))
		{
			for (int i = 0; i < numGCMPaths; i++)
			{
				TCHAR tmp[16];
				sprintf(tmp, "GCMPath%i", i);
				std::string tmpPath;
				ini.Get("General", tmp, &tmpPath, "");
				m_ISOFolder.push_back(tmpPath);
			}
		}

		ini.Get("General", "RecersiveGCMPaths",		&m_RecursiveISOFolder,							false);
	}

	{
		// Interface
		ini.Get("Interface", "ConfirmStop",			&m_LocalCoreStartupParameter.bConfirmStop,		false);
		ini.Get("Interface", "UsePanicHandlers",	&m_LocalCoreStartupParameter.bUsePanicHandlers,	true);
		ini.Get("Interface", "HideCursor",			&m_LocalCoreStartupParameter.bHideCursor,		false);
		ini.Get("Interface", "AutoHideCursor",		&m_LocalCoreStartupParameter.bAutoHideCursor,	false);
		ini.Get("Interface", "Theme",				&m_LocalCoreStartupParameter.iTheme,			0);
		ini.Get("Interface", "MainWindowPosX",		&m_LocalCoreStartupParameter.iPosX,				100);
		ini.Get("Interface", "MainWindowPosY",		&m_LocalCoreStartupParameter.iPosY,				100);
		ini.Get("Interface", "MainWindowWidth",		&m_LocalCoreStartupParameter.iWidth,			800);
		ini.Get("Interface", "MainWindowHeight",	&m_LocalCoreStartupParameter.iHeight,			600);
		ini.Get("Interface", "Language",			(int*)&m_InterfaceLanguage,						0);
		ini.Get("Interface", "ShowToolbar",			&m_InterfaceToolbar,							true);
		ini.Get("Interface", "ShowStatusbar",		&m_InterfaceStatusbar,							true);
		ini.Get("Interface", "ShowLogWindow",		&m_InterfaceLogWindow,							false);
		ini.Get("Interface", "ShowConsole",			&m_InterfaceConsole,							false);

		// Hotkeys
		ini.Get("Hotkeys", "ToggleFullscreen", 			&m_LocalCoreStartupParameter.iHotkey[HK_FULLSCREEN], 13); // WXK_RETURN
		ini.Get("Hotkeys", "PlayPause", 				&m_LocalCoreStartupParameter.iHotkey[HK_PLAY_PAUSE], 349); // WXK_F10
		ini.Get("Hotkeys", "Stop",						&m_LocalCoreStartupParameter.iHotkey[HK_STOP], 27); // WXK_ESCAPE
		ini.Get("Hotkeys", "ToggleFullscreenModifier",	&m_LocalCoreStartupParameter.iHotkeyModifier[HK_FULLSCREEN], 0x0001); // wxMOD_ALT
		ini.Get("Hotkeys", "PlayPauseModifier",			&m_LocalCoreStartupParameter.iHotkeyModifier[HK_PLAY_PAUSE], 0x0000); // wxMOD_NONE
		ini.Get("Hotkeys", "StopModifier",				&m_LocalCoreStartupParameter.iHotkeyModifier[HK_STOP], 0x0000); // wxMOD_NONE

		// Display
		ini.Get("Display", "Fullscreen",			&m_LocalCoreStartupParameter.bFullscreen,		false);
		ini.Get("Display", "RenderToMain",			&m_LocalCoreStartupParameter.bRenderToMain,		false);
		std::string temp;
		ini.Get("Display", "RenderWindowXPos",		&m_LocalCoreStartupParameter.iRenderWindowXPos,	0);
		ini.Get("Display", "RenderWindowYPos",		&m_LocalCoreStartupParameter.iRenderWindowYPos,	0);
		ini.Get("Display", "RenderWindowWidth",		&m_LocalCoreStartupParameter.iRenderWindowWidth, 640);
		ini.Get("Display", "RenderWindowHeight",	&m_LocalCoreStartupParameter.iRenderWindowHeight, 480);

		// Game List Control
		ini.Get("GameList", "ListDrives",	&m_ListDrives,	false);
		ini.Get("GameList", "ListWad",		&m_ListWad,		true);
		ini.Get("GameList", "ListWii",		&m_ListWii,		true);
		ini.Get("GameList", "ListGC",		&m_ListGC,		true);
		ini.Get("GameList", "ListJap",		&m_ListJap,		true);
		ini.Get("GameList", "ListPal",		&m_ListPal,		true);
		ini.Get("GameList", "ListUsa",		&m_ListUsa,		true);

		ini.Get("GameList", "ListFrance",		&m_ListFrance, true);
		ini.Get("GameList", "ListItaly",		&m_ListItaly, true);
		ini.Get("GameList", "ListKorea",		&m_ListKorea, true);
		ini.Get("GameList", "ListTaiwan",		&m_ListTaiwan, true);
		ini.Get("GameList", "ListUnknown",		&m_ListUnknown, true);

		// Core
		ini.Get("Core", "HLE_BS2",		&m_LocalCoreStartupParameter.bHLE_BS2,		true);
		ini.Get("Core", "CPUCore",		&m_LocalCoreStartupParameter.iCPUCore,		1);
		ini.Get("Core", "DSPThread",	&m_LocalCoreStartupParameter.bDSPThread,	false);
		ini.Get("Core", "CPUThread",	&m_LocalCoreStartupParameter.bCPUThread,	true);
		ini.Get("Core", "SkipIdle",		&m_LocalCoreStartupParameter.bSkipIdle,		true);
		ini.Get("Core", "LockThreads",	&m_LocalCoreStartupParameter.bLockThreads,	false);
		ini.Get("Core", "DefaultGCM",	&m_LocalCoreStartupParameter.m_strDefaultGCM);
		ini.Get("Core", "DVDRoot",		&m_LocalCoreStartupParameter.m_strDVDRoot);
		ini.Get("Core", "Apploader",	&m_LocalCoreStartupParameter.m_strApploader);
		ini.Get("Core", "EnableCheats",	&m_LocalCoreStartupParameter.bEnableCheats,				false);
		ini.Get("Core", "SelectedLanguage", &m_LocalCoreStartupParameter.SelectedLanguage,		0);
		ini.Get("Core", "MemcardA",		&m_strMemoryCardA);
		ini.Get("Core", "MemcardB",		&m_strMemoryCardB);
		ini.Get("Core", "SlotA",		(int*)&m_EXIDevice[0], EXIDEVICE_MEMORYCARD_A);
		ini.Get("Core", "SlotB",		(int*)&m_EXIDevice[1], EXIDEVICE_MEMORYCARD_B);
		ini.Get("Core", "SerialPort1",	(int*)&m_EXIDevice[2], EXIDEVICE_NONE);
		ini.Get("Core", "ProfiledReJIT",&m_LocalCoreStartupParameter.bJITProfiledReJIT,			false);
		char sidevicenum[16];
		for (int i = 0; i < 4; ++i)
		{
			sprintf(sidevicenum, "SIDevice%i", i);
			ini.Get("Core", sidevicenum,	(u32*)&m_SIDevice[i], i==0 ? SI_GC_CONTROLLER:SI_NONE);
		}

		ini.Get("Core", "WiiSDCard", &m_WiiSDCard, false);
		ini.Get("Core", "WiiKeyboard", &m_WiiKeyboard, false);
		ini.Get("Core", "RunCompareServer",	&m_LocalCoreStartupParameter.bRunCompareServer,	false);
		ini.Get("Core", "RunCompareClient",	&m_LocalCoreStartupParameter.bRunCompareClient,	false);
		ini.Get("Core", "TLBHack",			&m_LocalCoreStartupParameter.iTLBHack,			0);
		ini.Get("Core", "FrameLimit",		&m_Framelimit,									1); // auto frame limit by default

		// Plugins
		ini.Get("Core", "GFXPlugin",  &m_LocalCoreStartupParameter.m_strVideoPlugin,	m_DefaultGFXPlugin.c_str());
		ini.Get("Core", "DSPPlugin",  &m_LocalCoreStartupParameter.m_strDSPPlugin,		m_DefaultDSPPlugin.c_str());
		ini.Get("Core", "PadPlugin", &m_LocalCoreStartupParameter.m_strPadPlugin[0], m_DefaultPADPlugin.c_str());
		ini.Get("Core", "WiiMotePlugin", &m_LocalCoreStartupParameter.m_strWiimotePlugin[0], m_DefaultWiiMotePlugin.c_str());


	}

	m_SYSCONF = new SysConf();
}
void SConfig::LoadSettingsWii()
{
	IniFile ini;
	//Wiimote configs
	ini.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "Dolphin.ini").c_str());
	for (int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);
		ini.Get(SectionName, "AutoReconnectRealWiimote", &m_WiiAutoReconnect[i], false);
	}
}

// Is this still even needed????
void SConfig::LoadSettingsHLE()
{
	IniFile ini;
	ini.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "DSP.ini").c_str());
	ini.Get("Config", "EnableRE0AudioFix", &m_EnableRE0Fix, false); // RE0 Hack
}
