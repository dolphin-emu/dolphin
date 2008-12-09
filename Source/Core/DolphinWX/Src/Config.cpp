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

#include "Globals.h"
#include "Common.h"
#include "IniFile.h"
#include "Config.h"
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
        //	ini.Load(CONFIG_FILE); // yes we must load first to not kill unknown stuff

	// misc
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

	// core
	{
		ini.Set("Core", "GFXPlugin",  m_LocalCoreStartupParameter.m_strVideoPlugin);
		ini.Set("Core", "DSPPlugin",  m_LocalCoreStartupParameter.m_strDSPPlugin);
		ini.Set("Core", "PadPlugin",  m_LocalCoreStartupParameter.m_strPadPlugin);
		ini.Set("Core", "WiiMotePlugin",  m_LocalCoreStartupParameter.m_strWiimotePlugin);

		ini.Set("Core", "HLEBios",        m_LocalCoreStartupParameter.bHLEBios);
		ini.Set("Core", "UseDynarec",     m_LocalCoreStartupParameter.bUseJIT);
		ini.Set("Core", "UseDualCore",    m_LocalCoreStartupParameter.bUseDualCore);
		ini.Set("Core", "SkipIdle",	      m_LocalCoreStartupParameter.bSkipIdle);
		ini.Set("Core", "LockThreads",    m_LocalCoreStartupParameter.bLockThreads);
		ini.Set("Core", "DefaultGCM",     m_LocalCoreStartupParameter.m_strDefaultGCM);
		ini.Set("Core", "DVDRoot",        m_LocalCoreStartupParameter.m_strDVDRoot);
		ini.Set("Core", "OptimizeQuantizers", m_LocalCoreStartupParameter.bOptimizeQuantizers);
		ini.Set("Core", "EnableCheats", m_LocalCoreStartupParameter.bEnableCheats);
		ini.Set("Core", "SelectedLanguage", m_LocalCoreStartupParameter.SelectedLanguage);
		ini.Set("Core", "RunCompareServer", m_LocalCoreStartupParameter.bRunCompareServer);
		ini.Set("Core", "RunCompareClient", m_LocalCoreStartupParameter.bRunCompareClient);

		ini.Set("Wii", "ShowWiimoteLeds", m_LocalCoreStartupParameter.bWiiLeds);
		ini.Set("Wii", "ShowWiimoteSpeakers", m_LocalCoreStartupParameter.bWiiSpeakers);
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
	// hard coded default plugin
	{
            m_DefaultGFXPlugin = PLUGINS_DIR DIR_SEP DEFAULT_GFX_PLUGIN;
            m_DefaultDSPPlugin = PLUGINS_DIR DIR_SEP DEFAULT_DSP_PLUGIN; 
            m_DefaultPADPlugin = PLUGINS_DIR DIR_SEP DEFAULT_PAD_PLUGIN; 
            m_DefaultWiiMotePlugin = PLUGINS_DIR DIR_SEP DEFAULT_WIIMOTE_PLUGIN;  
	}
#endif
	// misc
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

	// core
	{
		ini.Get("Core", "GFXPlugin",  &m_LocalCoreStartupParameter.m_strVideoPlugin, m_DefaultGFXPlugin.c_str());
		ini.Get("Core", "DSPPlugin",  &m_LocalCoreStartupParameter.m_strDSPPlugin, m_DefaultDSPPlugin.c_str());
		ini.Get("Core", "PadPlugin",  &m_LocalCoreStartupParameter.m_strPadPlugin, m_DefaultPADPlugin.c_str());
		ini.Get("Core", "WiiMotePlugin",  &m_LocalCoreStartupParameter.m_strWiimotePlugin, m_DefaultWiiMotePlugin.c_str());
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
		ini.Get("Core", "RunCompareServer", &m_LocalCoreStartupParameter.bRunCompareServer, false);
		ini.Get("Core", "RunCompareClient", &m_LocalCoreStartupParameter.bRunCompareClient, false);

		ini.Get("Wii", "ShowWiimoteLeds", &m_LocalCoreStartupParameter.bWiiLeds, false);
		ini.Get("Wii", "ShowWiimoteSpeakers", &m_LocalCoreStartupParameter.bWiiSpeakers, false);
		
	}
}
