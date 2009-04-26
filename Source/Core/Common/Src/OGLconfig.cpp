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

#include "Common.h"
#include "IniFile.h"
#include "OGLconfig.h"
#include "..\..\core\src\ConfigManager.h" // FIXME

OGLConfig ogl_Config;

OGLConfig::OGLConfig()
{
    memset(this, 0, sizeof(OGLConfig));
}

void OGLConfig::Load()
{
    std::string temp;
    IniFile iniFile;
    iniFile.Load(FULL_CONFIG_DIR "gfx_opengl.ini");

	// get resolution
    iniFile.Get("Hardware", "WindowedRes", &temp, "640x480");
    strncpy(iWindowedRes, temp.c_str(), 16);

    iniFile.Get("Hardware", "FullscreenRes", &temp, "640x480");
    strncpy(iFSResolution, temp.c_str(), 16);
    
    iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0); // Hardware
    iniFile.Get("Hardware", "VSync", &bVSync, 0); // Hardware
    iniFile.Get("Hardware", "RenderToMainframe", &renderToMainframe, false);
    iniFile.Get("Settings", "StretchToFit", &bNativeResolution, true);
	iniFile.Get("Settings", "KeepAR_4_3", &bKeepAR43, false);
	iniFile.Get("Settings", "KeepAR_16_9", &bKeepAR169, false);
	iniFile.Get("Settings", "Crop", &bCrop, false);
    iniFile.Get("Settings", "HideCursor", &bHideCursor, false);
    iniFile.Get("Settings", "UseXFB", &bUseXFB, 0);
    iniFile.Get("Settings", "AutoScale", &bAutoScale, true);
    
    iniFile.Get("Settings", "SafeTextureCache", &bSafeTextureCache, false); // Settings
    iniFile.Get("Settings", "ShowFPS", &bShowFPS, false); // Settings
    iniFile.Get("Settings", "OverlayStats", &bOverlayStats, false);
	iniFile.Get("Settings", "OverlayBlendStats", &bOverlayBlendStats, false);
	iniFile.Get("Settings", "OverlayProjStats", &bOverlayProjStats, false);
	iniFile.Get("Settings", "ShowEFBCopyRegions", &bShowEFBCopyRegions, false);
    iniFile.Get("Settings", "DLOptimize", &iCompileDLsLevel, 0);
    iniFile.Get("Settings", "DumpTextures", &bDumpTextures, 0);
	iniFile.Get("Settings", "DumpEFBTarget", &bDumpEFBTarget, 0);
	iniFile.Get("Settings", "DumpFrames", &bDumpFrames, 0);
    iniFile.Get("Settings", "FreeLook", &bFreeLook, 0);
    iniFile.Get("Settings", "ShowShaderErrors", &bShowShaderErrors, 0);
    iniFile.Get("Settings", "MSAA", &iMultisampleMode, 0);
    iniFile.Get("Settings", "DstAlphaPass", &bDstAlphaPass, false);
    
    iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
    iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
    iniFile.Get("Settings", "WireFrame", &bWireFrame, 0);
    iniFile.Get("Settings", "DisableLighting", &bDisableLighting, 0);
    iniFile.Get("Settings", "DisableTexturing", &bDisableTexturing, 0);
	iniFile.Get("Settings", "DisableFog", &bDisableFog, 0);
    
    iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
    iniFile.Get("Enhancements", "MaxAnisotropy", &iMaxAnisotropy, 3);  // NOTE - this is x in (1 << x)
    
    iniFile.Get("Hacks", "EFBCopyDisable", &bEFBCopyDisable, 0);
    iniFile.Get("Hacks", "EFBCopyDisableHotKey", &bEFBCopyDisableHotKey, 0);
    iniFile.Get("Hacks", "ProjectionHax1", &bProjectionHax1, 0);
	iniFile.Get("Hacks", "EFBToTextureEnable", &bCopyEFBToRAM, 0);
	iniFile.Get("Hacks", "SMGhack", &bSMGhack, false);
}


