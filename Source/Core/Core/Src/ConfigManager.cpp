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
#include "CommonPaths.h"
#include "IniFile.h"
#include "ConfigManager.h"
#include "PluginManager.h"
#include "FileUtil.h"

SConfig* SConfig::m_Instance;

static const struct {
	const char*	IniText;
	const int	DefaultKey;
	const int	DefaultModifier;
} g_HKData[] = {
#ifdef __APPLE__
	{ "PlayPause",		 80 /* 'P' */,		0x08 /* wxMOD_CMD */ },
	{ "Stop",		 87 /* 'W' */,		0x08 /* wxMOD_CMD */ },
	{ "Screenshot",		 83 /* 'S' */,		0x08 /* wxMOD_CMD */ },
	{ "ToggleFullscreen",	 70 /* 'F' */,		0x08 /* wxMOD_CMD */ },
	{ "Wiimote1Connect", 	 49 /* '1' */,		0x08 /* wxMOD_CMD */ },
	{ "Wiimote2Connect", 	 50 /* '2' */,		0x08 /* wxMOD_CMD */ },
	{ "Wiimote3Connect", 	 51 /* '3' */,		0x08 /* wxMOD_CMD */ },
	{ "Wiimote4Connect", 	 52 /* '4' */,		0x08 /* wxMOD_CMD */ },
#else
	{ "PlayPause",		349 /* WXK_F10 */,	0x00 /* wxMOD_NONE*/ },
	{ "Stop",		 27 /* WXK_ESCAPE */,	0x00 /* wxMOD_NONE*/ },
	{ "Screenshot",		348 /* WXK_F9 */,	0x00 /* wxMOD_NONE*/ },
	{ "ToggleFullscreen",	 13 /* WXK_RETURN */,	0x01 /* wxMOD_ALT */ },
	{ "Wiimote1Connect", 	344 /* WXK_F5 */,	0x01 /* wxMOD_ALT */ },
	{ "Wiimote2Connect", 	345 /* WXK_F6 */,	0x01 /* wxMOD_ALT */ },
	{ "Wiimote3Connect", 	346 /* WXK_F7 */,	0x01 /* wxMOD_ALT */ },
	{ "Wiimote4Connect", 	347 /* WXK_F8 */,	0x01 /* wxMOD_ALT */ },
#endif
};

SConfig::SConfig()
{
	// Make sure we have log manager
	LoadSettings();
	//Make sure we load any extra settings
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
	NOTICE_LOG(BOOT, "Saving settings to %s", File::GetUserPath(F_DOLPHINCONFIG_IDX));
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

	ini.Set("General", "RecursiveGCMPaths", m_RecursiveISOFolder);

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
	for (int i = HK_FULLSCREEN; i < NUM_HOTKEYS; i++)
	{
		ini.Set("Hotkeys", g_HKData[i].IniText, m_LocalCoreStartupParameter.iHotkey[i]);
		ini.Set("Hotkeys", (std::string(g_HKData[i].IniText) + "Modifier").c_str(),
				m_LocalCoreStartupParameter.iHotkeyModifier[i]);
	}

	// Display
	ini.Set("Display", "FullscreenResolution",	m_LocalCoreStartupParameter.strFullscreenResolution);
	ini.Set("Display", "Fullscreen",			m_LocalCoreStartupParameter.bFullscreen);
	ini.Set("Display", "RenderToMain",			m_LocalCoreStartupParameter.bRenderToMain);
	ini.Set("Display", "RenderWindowXPos",		m_LocalCoreStartupParameter.iRenderWindowXPos);
	ini.Set("Display", "RenderWindowYPos",		m_LocalCoreStartupParameter.iRenderWindowYPos);
	ini.Set("Display", "RenderWindowWidth",		m_LocalCoreStartupParameter.iRenderWindowWidth);
	ini.Set("Display", "RenderWindowHeight",	m_LocalCoreStartupParameter.iRenderWindowHeight);
	ini.Set("Display", "RenderWindowAutoSize",	m_LocalCoreStartupParameter.bRenderWindowAutoSize);
	ini.Set("Display", "ProgressiveScan",		m_LocalCoreStartupParameter.bProgressive);
	ini.Set("Display", "NTSCJ",					m_LocalCoreStartupParameter.bNTSCJ);

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
	ini.Set("Core", "DSPHLE",			m_LocalCoreStartupParameter.bDSPHLE);
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
	ini.Set("Core", "BBA_MAC",			m_bba_mac);
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
	ini.Set("Core", "UseFPS",		b_UseFPS);

	// Plugins
	ini.Set("Core", "GFXPlugin",	m_LocalCoreStartupParameter.m_strVideoPlugin);

	ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	m_SYSCONF->Save();
}


void SConfig::LoadSettings()
{
	INFO_LOG(BOOT, "Loading Settings from %s", File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));

	// Hard coded defaults
	m_DefaultGFXPlugin = DEFAULT_GFX_PLUGIN;
	m_DefaultDSPPlugin = DEFAULT_DSP_PLUGIN;

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

		ini.Get("General", "RecursiveGCMPaths",		&m_RecursiveISOFolder,							false);
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
		ini.Get("Interface", "Language",			&m_InterfaceLanguage,							0);
		ini.Get("Interface", "ShowToolbar",			&m_InterfaceToolbar,							true);
		ini.Get("Interface", "ShowStatusbar",		&m_InterfaceStatusbar,							true);
		ini.Get("Interface", "ShowLogWindow",		&m_InterfaceLogWindow,							false);
		ini.Get("Interface", "ShowConsole",			&m_InterfaceConsole,							false);

		// Hotkeys
		for (int i = HK_FULLSCREEN; i < NUM_HOTKEYS; i++)
		{
			ini.Get("Hotkeys", g_HKData[i].IniText,
					&m_LocalCoreStartupParameter.iHotkey[i], g_HKData[i].DefaultKey);
			ini.Get("Hotkeys", (std::string(g_HKData[i].IniText) + "Modifier").c_str(),
					&m_LocalCoreStartupParameter.iHotkeyModifier[i], g_HKData[i].DefaultModifier);
		}

		// Display
		ini.Get("Display", "Fullscreen",			&m_LocalCoreStartupParameter.bFullscreen,		false);
		ini.Get("Display", "FullscreenResolution",	&m_LocalCoreStartupParameter.strFullscreenResolution, "640x480");
		ini.Get("Display", "RenderToMain",			&m_LocalCoreStartupParameter.bRenderToMain,		false);
		ini.Get("Display", "RenderWindowXPos",		&m_LocalCoreStartupParameter.iRenderWindowXPos,	-1);
		ini.Get("Display", "RenderWindowYPos",		&m_LocalCoreStartupParameter.iRenderWindowYPos,	-1);
		ini.Get("Display", "RenderWindowWidth",		&m_LocalCoreStartupParameter.iRenderWindowWidth, 640);
		ini.Get("Display", "RenderWindowHeight",	&m_LocalCoreStartupParameter.iRenderWindowHeight, 480);
		ini.Get("Display", "RenderWindowAutoSize",	&m_LocalCoreStartupParameter.bRenderWindowAutoSize, false);
		ini.Get("Display", "ProgressiveScan",		&m_LocalCoreStartupParameter.bProgressive, false);
		ini.Get("Display", "NTSCJ",					&m_LocalCoreStartupParameter.bNTSCJ, false);

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
		ini.Get("Core", "HLE_BS2",		&m_LocalCoreStartupParameter.bHLE_BS2,		false);
		ini.Get("Core", "CPUCore",		&m_LocalCoreStartupParameter.iCPUCore,		1);
		ini.Get("Core", "DSPThread",	&m_LocalCoreStartupParameter.bDSPThread,	false);
		ini.Get("Core", "DSPHLE",		&m_LocalCoreStartupParameter.bDSPHLE,		true);
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
		ini.Get("Core", "SlotB",		(int*)&m_EXIDevice[1], EXIDEVICE_NONE);
		ini.Get("Core", "SerialPort1",	(int*)&m_EXIDevice[2], EXIDEVICE_NONE);
		ini.Get("Core", "BBA_MAC",		&m_bba_mac);
		ini.Get("Core", "ProfiledReJIT",&m_LocalCoreStartupParameter.bJITProfiledReJIT,			false);
		ini.Get("Core", "TimeProfiling",&m_LocalCoreStartupParameter.bJITILTimeProfiling,		false);
		ini.Get("Core", "OutputIR",		&m_LocalCoreStartupParameter.bJITILOutputIR,			false);
		char sidevicenum[16];
		for (int i = 0; i < 4; ++i)
		{
			sprintf(sidevicenum, "SIDevice%i", i);
			ini.Get("Core", sidevicenum,	(u32*)&m_SIDevice[i], i==0 ? SI_GC_CONTROLLER:SI_NONE);
		}

		ini.Get("Core", "WiiSDCard",		&m_WiiSDCard,									false);
		ini.Get("Core", "WiiKeyboard",		&m_WiiKeyboard,									false);
		ini.Get("Core", "RunCompareServer",	&m_LocalCoreStartupParameter.bRunCompareServer,	false);
		ini.Get("Core", "RunCompareClient",	&m_LocalCoreStartupParameter.bRunCompareClient,	false);
		ini.Get("Core", "MMU",				&m_LocalCoreStartupParameter.bMMU,				false);
		ini.Get("Core", "TLBHack",			&m_LocalCoreStartupParameter.iTLBHack,			0);
		ini.Get("Core", "VBeam",			&m_LocalCoreStartupParameter.bVBeam,			false);
		ini.Get("Core", "FastDiscSpeed",	&m_LocalCoreStartupParameter.bFastDiscSpeed,	false);
		ini.Get("Core", "BAT",				&m_LocalCoreStartupParameter.bMMUBAT,			false);
		ini.Get("Core", "FrameLimit",		&m_Framelimit,									1); // auto frame limit by default
		ini.Get("Core", "UseFPS",			&b_UseFPS,										false); // use vps as default

		// Plugins
		ini.Get("Core", "GFXPlugin",  &m_LocalCoreStartupParameter.m_strVideoPlugin,	m_DefaultGFXPlugin.c_str());
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
		ini.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "wiimote.ini").c_str());
		ini.Get("Real", "Unpair", &m_WiiAutoUnpair, false);
		
}
