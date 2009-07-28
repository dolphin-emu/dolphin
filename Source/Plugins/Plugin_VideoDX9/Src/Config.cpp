// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Config.h"
#include "IniFile.h"

Config g_Config;

Config::Config()
{
}

void Config::Load()
{
	IniFile iniFile;
	iniFile.Load(FULL_CONFIG_DIR "gfx_dx9.ini");
	iniFile.Get("Hardware", "Adapter", &iAdapter, 0);
	iniFile.Get("Hardware", "WindowedRes", &iWindowedRes, 0);
	iniFile.Get("Hardware", "FullscreenRes", &iFSResolution, 0);
	iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0);
	iniFile.Get("Hardware", "RenderInMainframe", &renderToMainframe, false);
	iniFile.Get("Hardware", "VSync", &bVsync, 0);
	if (iAdapter == -1) 
		iAdapter = 0;

	iniFile.Get("Settings", "OverlayStats", &bOverlayStats, false);
	iniFile.Get("Settings", "OverlayProjection", &bOverlayProjStats, false);
	iniFile.Get("Settings", "Postprocess", &iPostprocessEffect, 0);
	iniFile.Get("Settings", "DumpTextures", &bDumpTextures, 0);
	iniFile.Get("Settings", "DumpFrames", &bDumpFrames, 0);
	iniFile.Get("Settings", "ShowShaderErrors", &bShowShaderErrors, 0);
	iniFile.Get("Settings", "Multisample", &iMultisampleMode, 0);
	iniFile.Get("Settings", "TexDumpPath", &texDumpPath, 0);

	iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);

	iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
	iniFile.Get("Enhancements", "ForceMaxAniso", &bForceMaxAniso, 0);

}

void Config::Save()
{
	IniFile iniFile;
	iniFile.Load(FULL_CONFIG_DIR "gfx_dx9.ini");
	iniFile.Set("Hardware", "Adapter", iAdapter);
	iniFile.Set("Hardware", "WindowedRes", iWindowedRes);
	iniFile.Set("Hardware", "FullscreenRes", iFSResolution);
	iniFile.Set("Hardware", "Fullscreen", bFullscreen);
	iniFile.Set("Hardware", "VSync", bVsync);
	iniFile.Set("Hardware", "RenderInMainframe", renderToMainframe);

	iniFile.Set("Settings", "OverlayStats", bOverlayStats);
	iniFile.Set("Settings", "OverlayProjection", bOverlayProjStats);
	iniFile.Set("Settings", "Postprocess", iPostprocessEffect);
	iniFile.Set("Settings", "DumpTextures", bDumpTextures);
	iniFile.Set("Settings", "DumpFrames", bDumpFrames);
	iniFile.Set("Settings", "ShowShaderErrors", bShowShaderErrors);
	iniFile.Set("Settings", "Multisample", iMultisampleMode);
	iniFile.Set("Settings", "TexDumpPath", texDumpPath);

	iniFile.Set("Settings", "TexFmtOverlayEnable", bTexFmtOverlayEnable);
	iniFile.Set("Settings", "TexFmtOverlayCenter", bTexFmtOverlayCenter);

	iniFile.Set("Enhancements", "ForceFiltering", bForceFiltering);
	iniFile.Set("Enhancements", "ForceMaxAniso", bForceMaxAniso);
	iniFile.Save(FULL_CONFIG_DIR "gfx_dx9.ini");
}
