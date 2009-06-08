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

#include "Globals.h"
#include "Common.h"
#include "IniFile.h"
#include "Config.h"
#include "../../../Core/Core/Src/ConfigManager.h" // FIXME

Config g_Config;

Config::Config()
{
    memset(this, 0, sizeof(Config));
}

void Config::Load()
{
    std::string temp;
    IniFile iniFile;
    iniFile.Load(FULL_CONFIG_DIR "gfx_opengl.ini");

	// get resolution
    iniFile.Get("Hardware", "WindowedRes", &temp, "640x480");
    strncpy(iInternalRes, temp.c_str(), 16);
	// apply this to the fullscreen resolution too
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
    iniFile.Get("Settings", "HiresTextures", &bHiresTextures, 0);
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
	iniFile.Get("Hacks", "EFBToTextureEnable", &bCopyEFBToRAM, 0);
	iniFile.Get("Hacks", "ProjectionHack", &iPhackvalue, 0);

	// Load common settings
	iniFile.Load(FULL_CONFIG_DIR "Dolphin.ini");
	bool bTmp;
	iniFile.Get("Interface", "UsePanicHandlers", &bTmp, true);
	SetEnableAlert(bTmp);
}

void Config::GameIniLoad() {
	IniFile *iniFile = ((struct SConfig *)globals->config)->m_LocalCoreStartupParameter.gameIni;
	if (! iniFile) 
		return;
	
	if (iniFile->Exists("Video", "ForceFiltering"))
		iniFile->Get("Video", "ForceFiltering", &bForceFiltering, 0);

	if (iniFile->Exists("Video", "MaxAnisotropy"))
		iniFile->Get("Video", "MaxAnisotropy", &iMaxAnisotropy, 3);  // NOTE - this is x in (1 << x)
    
	if (iniFile->Exists("Video", "EFBCopyDisable"))
		iniFile->Get("Video", "EFBCopyDisable", &bEFBCopyDisable, 0);

	if (iniFile->Exists("Video", "EFBCopyDisableHotKey"))
		iniFile->Get("Video", "EFBCopyDisableHotKey", &bEFBCopyDisableHotKey, 0);

	if (iniFile->Exists("Video", "EFBToTextureEnable"))
		iniFile->Get("Video", "EFBToTextureEnable", &bCopyEFBToRAM, 0);

	if (iniFile->Exists("Video", "SafeTextureCache"))
		iniFile->Get("Video", "SafeTextureCache", &bSafeTextureCache, false); 

	if (iniFile->Exists("Video", "MSAA"))
		iniFile->Get("Video", "MSAA", &iMultisampleMode, 0);

	if (iniFile->Exists("Video", "DstAlphaPass"))
		iniFile->Get("Video", "DstAlphaPass", &bDstAlphaPass, false);
	
	if (iniFile->Exists("Video", "UseXFB"))
		iniFile->Get("Video", "UseXFB", &bUseXFB, 0);

	if (iniFile->Exists("Video", "ProjectionHack"))
		iniFile->Get("Video", "ProjectionHack", &iPhackvalue, 0);
}

void Config::Save()
{
    IniFile iniFile;
    iniFile.Load(FULL_CONFIG_DIR "gfx_opengl.ini");
    iniFile.Set("Hardware", "WindowedRes", iInternalRes);
    iniFile.Set("Hardware", "FullscreenRes", iFSResolution);
    iniFile.Set("Hardware", "Fullscreen", bFullscreen);
    iniFile.Set("Hardware", "VSync", bVSync);
    iniFile.Set("Hardware", "RenderToMainframe", renderToMainframe);
    iniFile.Set("Settings", "StretchToFit", bNativeResolution);
    iniFile.Set("Settings", "KeepAR_4_3", bKeepAR43);
	iniFile.Set("Settings", "KeepAR_16_9", bKeepAR169);
	iniFile.Set("Settings", "Crop", bCrop);
    iniFile.Set("Settings", "HideCursor", bHideCursor);
    iniFile.Set("Settings", "UseXFB", bUseXFB);
    iniFile.Set("Settings", "AutoScale", bAutoScale);

    iniFile.Set("Settings", "SafeTextureCache", bSafeTextureCache);
    iniFile.Set("Settings", "ShowFPS", bShowFPS);
    iniFile.Set("Settings", "OverlayStats", bOverlayStats);
	iniFile.Set("Settings", "OverlayBlendStats", bOverlayBlendStats);
	iniFile.Set("Settings", "OverlayProjStats", bOverlayProjStats);
    iniFile.Set("Settings", "DLOptimize", iCompileDLsLevel);
	iniFile.Set("Settings", "Show", iCompileDLsLevel);
    iniFile.Set("Settings", "DumpTextures", bDumpTextures);
    iniFile.Set("Settings", "HiresTextures", bHiresTextures);
	iniFile.Set("Settings", "DumpEFBTarget", bDumpEFBTarget);
	iniFile.Set("Settings", "DumpFrames", bDumpFrames);
    iniFile.Set("Settings", "FreeLook", bFreeLook);
    iniFile.Set("Settings", "ShowEFBCopyRegions", bShowEFBCopyRegions);
	iniFile.Set("Settings", "ShowShaderErrors", bShowShaderErrors);
    iniFile.Set("Settings", "MSAA", iMultisampleMode);
    iniFile.Set("Settings", "TexFmtOverlayEnable", bTexFmtOverlayEnable);
    iniFile.Set("Settings", "TexFmtOverlayCenter", bTexFmtOverlayCenter);
    iniFile.Set("Settings", "Wireframe", bWireFrame);
    iniFile.Set("Settings", "DisableLighting", bDisableLighting);
    iniFile.Set("Settings", "DisableTexturing", bDisableTexturing);
    iniFile.Set("Settings", "DstAlphaPass", bDstAlphaPass);
	iniFile.Set("Settings", "DisableFog", bDisableFog);
    
    iniFile.Set("Enhancements", "ForceFiltering", bForceFiltering);
    iniFile.Set("Enhancements", "MaxAnisotropy", iMaxAnisotropy);
    
    iniFile.Set("Hacks", "EFBCopyDisable", bEFBCopyDisable);
    iniFile.Set("Hacks", "EFBCopyDisableHotKey", bEFBCopyDisableHotKey);
	iniFile.Set("Hacks", "EFBToTextureEnable", bCopyEFBToRAM);
	iniFile.Set("Hacks", "ProjectionHack", iPhackvalue);

    iniFile.Save(FULL_CONFIG_DIR "gfx_opengl.ini");
}

