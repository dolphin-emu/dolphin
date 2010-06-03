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

static const struct {
	const char*	IniText;
	const int DefaultKey;
	const int DefaultModifier;
} g_HKData[] = {
	{ "ToggleFullscreen",	13 /* WXK_RETURN */,	0x0001 /* wxMOD_ALT */ },
	{ "PlayPause",		 	349 /* WXK_F10 */,		0x0000 /* wxMOD_NONE */ },
	{ "Stop",			 	27 /* WXK_ESCAPE */,	0x0000 /* wxMOD_NONE */ },
	{ "Wiimote1Connect", 	344 /* WXK_F5 */,		0x0001 /* wxMOD_ALT */ },
	{ "Wiimote2Connect", 	345 /* WXK_F6 */,		0x0001 /* wxMOD_ALT */ },
	{ "Wiimote3Connect", 	346 /* WXK_F7 */,		0x0001 /* wxMOD_ALT */ },
	{ "Wiimote4Connect", 	347 /* WXK_F8 */,		0x0001 /* wxMOD_ALT */ },
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
	NOTICE_LOG(BOOT, "Saving Settings to %s", File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX)); // load first to not kill unknown stuff

	// General
	Section& general = ini["General"];
	general.Set("LastFilename", m_LastFilename);

	// ISO folders
	general.Set("GCMPathes", (int)m_ISOFolder.size());

	for (size_t i = 0; i < m_ISOFolder.size(); i++)
	{
		TCHAR tmp[16];
		sprintf(tmp, "GCMPath%i", (int)i);
		general.Set(tmp, m_ISOFolder[i]);
	}

	general.Set("RecersiveGCMPaths", m_RecursiveISOFolder);

	// Interface
	Section& iface = ini["Interface"];
	iface.Set("ConfirmStop",		m_LocalCoreStartupParameter.bConfirmStop);
	iface.Set("UsePanicHandlers",	m_LocalCoreStartupParameter.bUsePanicHandlers);
	iface.Set("HideCursor",			m_LocalCoreStartupParameter.bHideCursor);
	iface.Set("AutoHideCursor",		m_LocalCoreStartupParameter.bAutoHideCursor);
	iface.Set("Theme",				m_LocalCoreStartupParameter.iTheme);
	iface.Set("MainWindowPosX",		m_LocalCoreStartupParameter.iPosX);
	iface.Set("MainWindowPosY",		m_LocalCoreStartupParameter.iPosY);
	iface.Set("MainWindowWidth",	m_LocalCoreStartupParameter.iWidth);
	iface.Set("MainWindowHeight",	m_LocalCoreStartupParameter.iHeight);
	iface.Set("Language",			m_InterfaceLanguage);
	iface.Set("ShowToolbar",		m_InterfaceToolbar);
	iface.Set("ShowStatusbar",		m_InterfaceStatusbar);
	iface.Set("ShowLogWindow",		m_InterfaceLogWindow);
	iface.Set("ShowConsole",		m_InterfaceConsole);

	// Hotkeys
	Section& hotkeys = ini["Hotkeys"];
	for (int i = HK_FULLSCREEN; i < NUM_HOTKEYS; i++)
	{
		hotkeys.Set(g_HKData[i].IniText, m_LocalCoreStartupParameter.iHotkey[i]);
		hotkeys.Set((std::string(g_HKData[i].IniText) + "Modifier").c_str(),
				m_LocalCoreStartupParameter.iHotkeyModifier[i]);
	}

	// Display
	Section& display = ini["Display"];
	display.Set("FullscreenResolution",	m_LocalCoreStartupParameter.strFullscreenResolution);
	display.Set("Fullscreen",			m_LocalCoreStartupParameter.bFullscreen);
	display.Set("RenderToMain",			m_LocalCoreStartupParameter.bRenderToMain);
	display.Set("RenderWindowXPos",		m_LocalCoreStartupParameter.iRenderWindowXPos);
	display.Set("RenderWindowYPos",		m_LocalCoreStartupParameter.iRenderWindowYPos);
	display.Set("RenderWindowWidth",	m_LocalCoreStartupParameter.iRenderWindowWidth);
	display.Set("RenderWindowHeight",	m_LocalCoreStartupParameter.iRenderWindowHeight);

	// Game List Control
	Section& gamelist = ini["GameList"];
	gamelist.Set("ListDrives",	m_ListDrives);
	gamelist.Set("ListWad",		m_ListWad);
	gamelist.Set("ListWii",		m_ListWii);
	gamelist.Set("ListGC",		m_ListGC);
	gamelist.Set("ListJap",		m_ListJap);
	gamelist.Set("ListPal",		m_ListPal);
	gamelist.Set("ListUsa",		m_ListUsa);
	gamelist.Set("ListFrance",	m_ListFrance);
	gamelist.Set("ListItaly",	m_ListItaly);
	gamelist.Set("ListKorea",	m_ListKorea);
	gamelist.Set("ListTaiwan",	m_ListTaiwan);
	gamelist.Set("ListUnknown",	m_ListUnknown);

	// Core
	Section& core = ini["Core"];
	core.Set("HLE_BS2",			m_LocalCoreStartupParameter.bHLE_BS2);
	core.Set("CPUCore",			m_LocalCoreStartupParameter.iCPUCore);
	core.Set("CPUThread",		m_LocalCoreStartupParameter.bCPUThread);
	core.Set("DSPThread",		m_LocalCoreStartupParameter.bDSPThread);
	core.Set("SkipIdle",		m_LocalCoreStartupParameter.bSkipIdle);
	core.Set("LockThreads",		m_LocalCoreStartupParameter.bLockThreads);
	core.Set("DefaultGCM",		m_LocalCoreStartupParameter.m_strDefaultGCM);
	core.Set("DVDRoot",			m_LocalCoreStartupParameter.m_strDVDRoot);
	core.Set("Apploader",		m_LocalCoreStartupParameter.m_strApploader);
	core.Set("EnableCheats",	m_LocalCoreStartupParameter.bEnableCheats);
	core.Set("SelectedLanguage",m_LocalCoreStartupParameter.SelectedLanguage);
	core.Set("MemcardA",		m_strMemoryCardA);
	core.Set("MemcardB",		m_strMemoryCardB);
	core.Set("SlotA",			m_EXIDevice[0]);
	core.Set("SlotB",			m_EXIDevice[1]);
	core.Set("SerialPort1",		m_EXIDevice[2]);
	char sidevicenum[16];
	for (int i = 0; i < 4; ++i)
	{
		sprintf(sidevicenum, "SIDevice%i", i);
		core.Set(sidevicenum, m_SIDevice[i]);
	}

	core.Set("WiiSDCard", m_WiiSDCard);
	core.Set("WiiKeyboard", m_WiiKeyboard);
	core.Set("RunCompareServer",	m_LocalCoreStartupParameter.bRunCompareServer);
	core.Set("RunCompareClient",	m_LocalCoreStartupParameter.bRunCompareClient);
	core.Set("FrameLimit",			m_Framelimit);
	core.Set("UseFPS",				b_UseFPS);

	// Plugins
	core.Set("GFXPlugin",	m_LocalCoreStartupParameter.m_strVideoPlugin);
	core.Set("DSPPlugin",	m_LocalCoreStartupParameter.m_strDSPPlugin);
	core.Set("WiiMotePlugin",m_LocalCoreStartupParameter.m_strWiimotePlugin[0]);

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
	m_DefaultWiiMotePlugin = PluginsDir + DEFAULT_WIIMOTE_PLUGIN;

	// General
	{
		Section& general = ini["General"];
		general.Get("LastFilename",	&m_LastFilename);

		m_ISOFolder.clear();

		unsigned int numGCMPaths;
		general.Get("GCMPathes", &numGCMPaths, 0);
		for (unsigned int i = 0; i < numGCMPaths; i++)
		{
			TCHAR tmp[16];
			sprintf(tmp, "GCMPath%i", i);
			std::string tmpPath;
			general.Get(tmp, &tmpPath, "");
			m_ISOFolder.push_back(tmpPath);
		}

		general.Get("RecersiveGCMPaths",		&m_RecursiveISOFolder,							false);
	}

	{
		// Interface
		Section& iface = ini["Interface"];
		iface.Get("ConfirmStop",		&m_LocalCoreStartupParameter.bConfirmStop,		false);
		iface.Get("UsePanicHandlers",	&m_LocalCoreStartupParameter.bUsePanicHandlers,	true);
		iface.Get("HideCursor",			&m_LocalCoreStartupParameter.bHideCursor,		false);
		iface.Get("AutoHideCursor",		&m_LocalCoreStartupParameter.bAutoHideCursor,	false);
		iface.Get("Theme",				&m_LocalCoreStartupParameter.iTheme,			0);
		iface.Get("MainWindowPosX",		&m_LocalCoreStartupParameter.iPosX,				100);
		iface.Get("MainWindowPosY",		&m_LocalCoreStartupParameter.iPosY,				100);
		iface.Get("MainWindowWidth",	&m_LocalCoreStartupParameter.iWidth,			800);
		iface.Get("MainWindowHeight",	&m_LocalCoreStartupParameter.iHeight,			600);
		iface.Get("Language",			(int*)&m_InterfaceLanguage,						0);
		iface.Get("ShowToolbar",		&m_InterfaceToolbar,							true);
		iface.Get("ShowStatusbar",		&m_InterfaceStatusbar,							true);
		iface.Get("ShowLogWindow",		&m_InterfaceLogWindow,							false);
		iface.Get("ShowConsole",		&m_InterfaceConsole,							false);

		// Hotkeys
		Section& hotkeys = ini["Hotkeys"];
		for (int i = HK_FULLSCREEN; i < NUM_HOTKEYS; i++)
		{
			hotkeys.Get(g_HKData[i].IniText,
					&m_LocalCoreStartupParameter.iHotkey[i], g_HKData[i].DefaultKey);
			hotkeys.Get((std::string(g_HKData[i].IniText) + "Modifier").c_str(),
					&m_LocalCoreStartupParameter.iHotkeyModifier[i], g_HKData[i].DefaultModifier);
		}

		// Display
		Section& display = ini["Display"];
		display.Get("Fullscreen",			&m_LocalCoreStartupParameter.bFullscreen,		false);
		display.Get("FullscreenResolution",	&m_LocalCoreStartupParameter.strFullscreenResolution, "640x480");
		display.Get("RenderToMain",			&m_LocalCoreStartupParameter.bRenderToMain,		false);
		display.Get("RenderWindowXPos",		&m_LocalCoreStartupParameter.iRenderWindowXPos,	0);
		display.Get("RenderWindowYPos",		&m_LocalCoreStartupParameter.iRenderWindowYPos,	0);
		display.Get("RenderWindowWidth",	&m_LocalCoreStartupParameter.iRenderWindowWidth, 640);
		display.Get("RenderWindowHeight",	&m_LocalCoreStartupParameter.iRenderWindowHeight, 480);

		// Game List Control
		Section& gamelist = ini["GameList"];
		gamelist.Get("ListDrives",	&m_ListDrives,	false);
		gamelist.Get("ListWad",		&m_ListWad,		true);
		gamelist.Get("ListWii",		&m_ListWii,		true);
		gamelist.Get("ListGC",		&m_ListGC,		true);
		gamelist.Get("ListJap",		&m_ListJap,		true);
		gamelist.Get("ListPal",		&m_ListPal,		true);
		gamelist.Get("ListUsa",		&m_ListUsa,		true);

		gamelist.Get("ListFrance",	&m_ListFrance, true);
		gamelist.Get("ListItaly",	&m_ListItaly, true);
		gamelist.Get("ListKorea",	&m_ListKorea, true);
		gamelist.Get("ListTaiwan",	&m_ListTaiwan, true);
		gamelist.Get("ListUnknown",	&m_ListUnknown, true);

		// Core
		Section& core = ini["Core"];
		core.Get("HLE_BS2",			&m_LocalCoreStartupParameter.bHLE_BS2,		true);
		core.Get("CPUCore",			&m_LocalCoreStartupParameter.iCPUCore,		1);
		core.Get("DSPThread",		&m_LocalCoreStartupParameter.bDSPThread,	false);
		core.Get("CPUThread",		&m_LocalCoreStartupParameter.bCPUThread,	true);
		core.Get("SkipIdle",		&m_LocalCoreStartupParameter.bSkipIdle,		true);
		core.Get("LockThreads",		&m_LocalCoreStartupParameter.bLockThreads,	false);
		core.Get("DefaultGCM",		&m_LocalCoreStartupParameter.m_strDefaultGCM);
		core.Get("DVDRoot",			&m_LocalCoreStartupParameter.m_strDVDRoot);
		core.Get("Apploader",		&m_LocalCoreStartupParameter.m_strApploader);
		core.Get("EnableCheats",	&m_LocalCoreStartupParameter.bEnableCheats,			false);
		core.Get("SelectedLanguage", &m_LocalCoreStartupParameter.SelectedLanguage,		0);
		core.Get("MemcardA",		&m_strMemoryCardA);
		core.Get("MemcardB",		&m_strMemoryCardB);
		core.Get("SlotA",			(int*)&m_EXIDevice[0], EXIDEVICE_MEMORYCARD_A);
		core.Get("SlotB",			(int*)&m_EXIDevice[1], EXIDEVICE_MEMORYCARD_B);
		core.Get("SerialPort1",		(int*)&m_EXIDevice[2], EXIDEVICE_NONE);
		core.Get("ProfiledReJIT",	&m_LocalCoreStartupParameter.bJITProfiledReJIT,		false);
		char sidevicenum[16];
		for (int i = 0; i < 4; ++i)
		{
			sprintf(sidevicenum, "SIDevice%i", i);
			core.Get(sidevicenum, (u32*)&m_SIDevice[i], i==0 ? SI_GC_CONTROLLER:SI_NONE);
		}

		core.Get("WiiSDCard",			&m_WiiSDCard, false);
		core.Get("WiiKeyboard",			&m_WiiKeyboard, false);
		core.Get("RunCompareServer",	&m_LocalCoreStartupParameter.bRunCompareServer,	false);
		core.Get("RunCompareClient",	&m_LocalCoreStartupParameter.bRunCompareClient,	false);
		core.Get("TLBHack",				&m_LocalCoreStartupParameter.iTLBHack,			0);
		core.Get("FrameLimit",			&m_Framelimit,									1); // auto frame limit by default
		core.Get("UseFPS",				&b_UseFPS,									false); // use vps as default

		// Plugins
		core.Get("GFXPlugin",		&m_LocalCoreStartupParameter.m_strVideoPlugin,	m_DefaultGFXPlugin.c_str());
		core.Get("DSPPlugin",		&m_LocalCoreStartupParameter.m_strDSPPlugin,		m_DefaultDSPPlugin.c_str());
		core.Get("WiiMotePlugin",	&m_LocalCoreStartupParameter.m_strWiimotePlugin[0], m_DefaultWiiMotePlugin.c_str());


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
		ini[SectionName].Get("AutoReconnectRealWiimote", &m_WiiAutoReconnect[i], false);
	}
}
