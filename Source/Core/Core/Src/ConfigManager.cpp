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

#include "Common.h"
#include "IniFile.h"
#include "ConfigManager.h"
#ifdef __APPLE__
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFUrl.h>
#include <CoreFoundation/CFBundle.h>
#include <sys/param.h>

#endif

SConfig SConfig::m_Instance;


SConfig::SConfig()
{
	LoadSettings();
}


SConfig::~SConfig()
{
	SaveSettings();
}


void SConfig::SaveSettings()
{
	IniFile ini;
	ini.Load(CONFIG_FILE); // yes we must load first to not kill unknown stuff

	// General
	{
		ini.Set("General", "LastFilename",	m_LastFilename);

		// ISO folders
		ini.Set("General", "GCMPathes",		(int)m_ISOFolder.size());

		for (size_t i = 0; i < m_ISOFolder.size(); i++)
		{
			TCHAR tmp[16];
			sprintf(tmp, "GCMPath%i", (int)i);
			ini.Set("General", tmp, m_ISOFolder[i]);
		}
	}

	
	{
		// Interface		
		ini.Set("Interface", "ConfirmStop", m_LocalCoreStartupParameter.bConfirmStop);
		ini.Set("Interface", "HideCursor", m_LocalCoreStartupParameter.bHideCursor);
		ini.Set("Interface", "AutoHideCursor", m_LocalCoreStartupParameter.bAutoHideCursor);
		ini.Set("Interface", "Theme", m_LocalCoreStartupParameter.iTheme);
		ini.Set("Interface", "ShowWiimoteLeds", m_LocalCoreStartupParameter.bWiiLeds);
		ini.Set("Interface", "ShowWiimoteSpeakers", m_LocalCoreStartupParameter.bWiiSpeakers);
		// interface(UI) language
		ini.Set("Interface", "Language",	m_InterfaceLanguage);

		// Core
		ini.Set("Core", "HLEBios",			m_LocalCoreStartupParameter.bHLEBios);
		ini.Set("Core", "UseDynarec",		m_LocalCoreStartupParameter.bUseJIT);
		ini.Set("Core", "UseDualCore",		m_LocalCoreStartupParameter.bUseDualCore);
		ini.Set("Core", "SkipIdle",			m_LocalCoreStartupParameter.bSkipIdle);
		ini.Set("Core", "LockThreads",		m_LocalCoreStartupParameter.bLockThreads);
		ini.Set("Core", "DefaultGCM",		m_LocalCoreStartupParameter.m_strDefaultGCM);
		ini.Set("Core", "DVDRoot",			m_LocalCoreStartupParameter.m_strDVDRoot);
		ini.Set("Core", "OptimizeQuantizers", m_LocalCoreStartupParameter.bOptimizeQuantizers);
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
			ini.Set("Core", sidevicenum,	m_SIDevice[i]);
		}

		ini.Set("Core", "RunCompareServer",	m_LocalCoreStartupParameter.bRunCompareServer);
		ini.Set("Core", "RunCompareClient",	m_LocalCoreStartupParameter.bRunCompareClient);

		// Wii
		ini.Set("Wii", "Widescreen", &m_LocalCoreStartupParameter.bWidescreen);
		ini.Set("Wii", "ProgressiveScan", &m_LocalCoreStartupParameter.bProgressiveScan);

		// Plugins
		ini.Set("Core", "GFXPlugin",  m_LocalCoreStartupParameter.m_strVideoPlugin);
		ini.Set("Core", "DSPPlugin",  m_LocalCoreStartupParameter.m_strDSPPlugin);
		ini.Set("Core", "Pad1Plugin",  m_LocalCoreStartupParameter.m_strPadPlugin[0]);
		ini.Set("Core", "Pad2Plugin",  m_LocalCoreStartupParameter.m_strPadPlugin[1]);
		ini.Set("Core", "Pad3Plugin",  m_LocalCoreStartupParameter.m_strPadPlugin[2]);
		ini.Set("Core", "Pad4Plugin",  m_LocalCoreStartupParameter.m_strPadPlugin[3]);
		ini.Set("Core", "WiiMote1Plugin",  m_LocalCoreStartupParameter.m_strWiimotePlugin[0]);
	}

	ini.Save(CONFIG_FILE);
}


void SConfig::LoadSettings()
{
	IniFile ini;
	ini.Load(CONFIG_FILE);
#ifdef __APPLE__
	// Plugin path will be Dolphin.app/Contents/PlugIns
	CFURLRef BundleRef, PluginDirRef;
	// Get the main bundle for the app
	BundleRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
	PluginDirRef = CFBundleCopyBuiltInPlugInsURL(CFBundleGetMainBundle());
	CFStringRef BundlePath = CFURLCopyFileSystemPath(BundleRef, kCFURLPOSIXPathStyle);
	CFStringRef PluginDirPath = CFURLCopyFileSystemPath(PluginDirRef, kCFURLPOSIXPathStyle);
	char AppBundlePath[MAXPATHLEN], PluginPath[MAXPATHLEN];
	CFStringGetFileSystemRepresentation(BundlePath, AppBundlePath, sizeof(AppBundlePath));
	CFStringGetFileSystemRepresentation(PluginDirPath, PluginPath, sizeof(PluginPath));
//	printf("bundle path = %s %s\n", AppBundlePath, PluginPath);
	CFRelease(BundleRef);
	CFRelease(BundlePath);
	CFRelease(PluginDirRef);
	CFRelease(PluginDirPath);
	std::string PluginsDir = AppBundlePath;
	PluginsDir += DIR_SEP;
	PluginsDir += PluginPath;
	PluginsDir += DIR_SEP;
	
	m_DefaultGFXPlugin = PluginsDir + DEFAULT_GFX_PLUGIN;
	m_DefaultDSPPlugin = PluginsDir + DEFAULT_DSP_PLUGIN;
	m_DefaultPADPlugin = PluginsDir + DEFAULT_PAD_PLUGIN;
	m_DefaultWiiMotePlugin = PluginsDir + DEFAULT_WIIMOTE_PLUGIN;
	
#else
	// Hard coded default plugin
	{
		m_DefaultGFXPlugin = PLUGINS_DIR DIR_SEP DEFAULT_GFX_PLUGIN;
		m_DefaultDSPPlugin = PLUGINS_DIR DIR_SEP DEFAULT_DSP_PLUGIN;
		m_DefaultPADPlugin = PLUGINS_DIR DIR_SEP DEFAULT_PAD_PLUGIN;
		m_DefaultWiiMotePlugin = PLUGINS_DIR DIR_SEP DEFAULT_WIIMOTE_PLUGIN;
	}
#endif
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
	}

	{
		// Interface
		ini.Get("Interface", "ConfirmStop", &m_LocalCoreStartupParameter.bConfirmStop, false);
		ini.Get("Interface", "HideCursor", &m_LocalCoreStartupParameter.bHideCursor, false);
		ini.Get("Interface", "AutoHideCursor", &m_LocalCoreStartupParameter.bAutoHideCursor, false);
		ini.Get("Interface", "Theme", &m_LocalCoreStartupParameter.iTheme, 0);
		ini.Get("Interface", "ShowWiimoteLeds", &m_LocalCoreStartupParameter.bWiiLeds, false);
		ini.Get("Interface", "ShowWiimoteSpeakers", &m_LocalCoreStartupParameter.bWiiSpeakers, false);
		// interface(UI) language
		ini.Get("Interface", "Language", (int*)&m_InterfaceLanguage, 0);

		// Core
		ini.Get("Core", "HLEBios",     &m_LocalCoreStartupParameter.bHLEBios,		true);
		ini.Get("Core", "UseDynarec",  &m_LocalCoreStartupParameter.bUseJIT,		true);
		ini.Get("Core", "UseDualCore", &m_LocalCoreStartupParameter.bUseDualCore,	false);
		ini.Get("Core", "SkipIdle",    &m_LocalCoreStartupParameter.bSkipIdle,		true);
		ini.Get("Core", "LockThreads", &m_LocalCoreStartupParameter.bLockThreads,	true);
		ini.Get("Core", "DefaultGCM",  &m_LocalCoreStartupParameter.m_strDefaultGCM);
		ini.Get("Core", "DVDRoot",     &m_LocalCoreStartupParameter.m_strDVDRoot);
		ini.Get("Core", "OptimizeQuantizers", &m_LocalCoreStartupParameter.bOptimizeQuantizers, true);
		ini.Get("Core", "EnableCheats", &m_LocalCoreStartupParameter.bEnableCheats, false);
		ini.Get("Core", "SelectedLanguage", &m_LocalCoreStartupParameter.SelectedLanguage, 0);
		ini.Get("Core", "MemcardA",			&m_strMemoryCardA);
		ini.Get("Core", "MemcardB",			&m_strMemoryCardB);
		ini.Get("Core", "SlotA",			(int*)&m_EXIDevice[0], EXIDEVICE_MEMORYCARD_A);
		ini.Get("Core", "SlotB",			(int*)&m_EXIDevice[1], EXIDEVICE_MEMORYCARD_B);
		ini.Get("Core", "SerialPort1",		(int*)&m_EXIDevice[2], EXIDEVICE_DUMMY);
		char sidevicenum[16];
		for (int i = 0; i < 4; ++i)
		{
			sprintf(sidevicenum, "SIDevice%i", i);
			ini.Get("Core", sidevicenum,	(u32*)&m_SIDevice[i], i==0 ? SI_GC_CONTROLLER:SI_DUMMY);
		}

		ini.Get("Core", "RunCompareServer", &m_LocalCoreStartupParameter.bRunCompareServer, false);
		ini.Get("Core", "RunCompareClient", &m_LocalCoreStartupParameter.bRunCompareClient, false);
		ini.Get("Core", "TLBHack", &m_LocalCoreStartupParameter.iTLBHack, 0);

		// Wii
		ini.Get("Wii", "Widescreen", &m_LocalCoreStartupParameter.bWidescreen, false);
		ini.Get("Wii", "ProgressiveScan", &m_LocalCoreStartupParameter.bProgressiveScan, true);

		// Plugins
		ini.Get("Core", "GFXPlugin",  &m_LocalCoreStartupParameter.m_strVideoPlugin, m_DefaultGFXPlugin.c_str());
		ini.Get("Core", "DSPPlugin",  &m_LocalCoreStartupParameter.m_strDSPPlugin, m_DefaultDSPPlugin.c_str());
		for (int i = 0; i < MAXPADS; i++)
		{
			std::string TmpName = StringFromFormat("Pad%iPlugin", (i + 1));
			ini.Get("Core", TmpName.c_str(),  &m_LocalCoreStartupParameter.m_strPadPlugin[i], m_DefaultPADPlugin.c_str());
		}
		ini.Get("Core", "WiiMote1Plugin",  &m_LocalCoreStartupParameter.m_strWiimotePlugin[0], m_DefaultWiiMotePlugin.c_str());
	}
}
