#include "Globals.h"
#include "IniFile.h"

Config g_Config;

Config::Config()
{
}

void Config::Load()
{
	IniFile iniFile;
	iniFile.Load("flipper.ini");
	iniFile.Get("Hardware", "Adapter", &iAdapter, 0);
	iniFile.Get("Hardware", "WindowedRes", &iWindowedRes, 0);
	iniFile.Get("Hardware", "FullscreenRes", &iFSResolution, 0);
	iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0);
	iniFile.Get("Hardware", "VSync", &bVsync, 0);
	if (iAdapter == -1) 
		iAdapter = 0;

	iniFile.Get("Settings", "OverlayStats", &bOverlayStats, false);
	iniFile.Get("Settings", "Postprocess", &iPostprocessEffect, 0);
	iniFile.Get("Settings", "DLOptimize", &iCompileDLsLevel, 0);
	iniFile.Get("Settings", "DumpTextures", &bDumpTextures, 0);
	iniFile.Get("Settings", "ShowShaderErrors", &bShowShaderErrors, 0);
	iniFile.Get("Settings", "Multisample", &iMultisampleMode, 0);
	iniFile.Get("Settings", "TexDumpPath", &texDumpPath, 0);
	
	iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
	iniFile.Get("Enhancements", "ForceMaxAniso", &bForceMaxAniso, 0);
}

void Config::Save()
{
	IniFile iniFile;
	iniFile.Load("flipper.ini");
	iniFile.Set("Hardware", "Adapter", iAdapter);
	iniFile.Set("Hardware", "WindowedRes", iWindowedRes);
	iniFile.Set("Hardware", "FullscreenRes", iFSResolution);
	iniFile.Set("Hardware", "Fullscreen", bFullscreen);
	iniFile.Set("Hardware", "VSync", bVsync);

	iniFile.Set("Settings", "OverlayStats", bOverlayStats);
	iniFile.Set("Settings", "OverlayStats", bOverlayStats);
	iniFile.Set("Settings", "Postprocess", iPostprocessEffect);
	iniFile.Set("Settings", "DLOptimize", iCompileDLsLevel);
	iniFile.Set("Settings", "DumpTextures", bDumpTextures);
	iniFile.Set("Settings", "ShowShaderErrors", bShowShaderErrors);
	iniFile.Set("Settings", "Multisample", iMultisampleMode);
	iniFile.Set("Settings", "TexDumpPath", texDumpPath);

	iniFile.Set("Enhancements", "ForceFiltering", bForceFiltering);
	iniFile.Set("Enhancements", "ForceMaxAniso", bForceMaxAniso);
	iniFile.Save("flipper.ini");
}

Statistics stats;