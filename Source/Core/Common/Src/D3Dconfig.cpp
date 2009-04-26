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

#include "D3DConfig.h"
#include "IniFile.h"

D3DConfig d3d_Config;

D3DConfig::D3DConfig()
{
	memset(this, 0, sizeof(D3DConfig));
}

void D3DConfig::Load()
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

