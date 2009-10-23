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

#include <cmath>

#include "Common.h"
#include "IniFile.h"
#include "VideoConfig.h"
#include "VideoCommon.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;

void UpdateActiveConfig() 
{
	g_ActiveConfig = g_Config;
}

VideoConfig::VideoConfig()
{
	bRunning = false;
	bAllowSignedBytes = !IsD3D();
}

void VideoConfig::Load(const char *ini_file)
{
    std::string temp;
    IniFile iniFile;
    iniFile.Load(ini_file);

	// get resolution
    iniFile.Get("Hardware", "WindowedRes", &temp, "640x480");
    strncpy(cInternalRes, temp.c_str(), 16);
	iniFile.Get("Hardware", "FullscreenRes", &temp, "640x480");
    strncpy(cFSResolution, temp.c_str(), 16);
    
    iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0); // Hardware
    iniFile.Get("Hardware", "VSync", &bVSync, 0); // Hardware
    iniFile.Get("Hardware", "RenderToMainframe", &RenderToMainframe, false);
    iniFile.Get("Settings", "StretchToFit", &bNativeResolution, true);
	iniFile.Get("Settings", "2xResolution", &b2xResolution, false);
	iniFile.Get("Settings", "wideScreenHack", &bWidescreenHack, false);
	iniFile.Get("Settings", "KeepAR_4_3", &bKeepAR43, true);
	iniFile.Get("Settings", "KeepAR_16_9", &bKeepAR169, false);
	iniFile.Get("Settings", "Crop", &bCrop, false);
    iniFile.Get("Settings", "HideCursor", &bHideCursor, false);
    iniFile.Get("Settings", "UseXFB", &bUseXFB, 0);
    iniFile.Get("Settings", "AutoScale", &bAutoScale, true);
    
    iniFile.Get("Settings", "SafeTextureCache", &bSafeTextureCache, false); // Settings
    iniFile.Get("Settings", "ShowFPS", &bShowFPS, false); // Settings
    iniFile.Get("Settings", "OverlayStats", &bOverlayStats, false);
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
    iniFile.Get("Enhancements", "MaxAnisotropy", &iMaxAnisotropy, 1);  // NOTE - this is x in (1 << x)
	iniFile.Get("Enhancements", "PostProcessingShader", &sPostProcessingShader, "");
    
    iniFile.Get("Hacks", "EFBAccessEnable", &bEFBAccessEnable, true);
    iniFile.Get("Hacks", "EFBCopyDisable", &bEFBCopyDisable, 0);
    iniFile.Get("Hacks", "EFBCopyDisableHotKey", &bOSDHotKey, 0);
	iniFile.Get("Hacks", "EFBToTextureEnable", &bCopyEFBToRAM, true);
	iniFile.Get("Hacks", "ProjectionHack", &iPhackvalue, 0);

	iniFile.Get("Hardware", "Adapter", &iAdapter, 0);
	if (iAdapter == -1) 
		iAdapter = 0;
	iniFile.Get("Hardware", "SimpleFB", &bSimpleFB, false);

	// Load common settings
	iniFile.Load(CONFIG_FILE);
	bool bTmp;
	iniFile.Get("Interface", "UsePanicHandlers", &bTmp, true);
	SetEnableAlert(bTmp);
}

void VideoConfig::GameIniLoad(const char *ini_file)
{
    IniFile iniFile;
    iniFile.Load(ini_file);

	if (iniFile.Exists("Video", "ForceFiltering"))
		iniFile.Get("Video", "ForceFiltering", &bForceFiltering, 0);
	if (iniFile.Exists("Video", "MaxAnisotropy"))
		iniFile.Get("Video", "MaxAnisotropy", &iMaxAnisotropy, 3);  // NOTE - this is x in (1 << x)
	if (iniFile.Exists("Video", "EFBCopyDisable"))
		iniFile.Get("Video", "EFBCopyDisable", &bEFBCopyDisable, 0);
	if (iniFile.Exists("Video", "EFBCopyDisableHotKey"))
		iniFile.Get("Video", "EFBCopyDisableHotKey", &bOSDHotKey, 0);
	if (iniFile.Exists("Video", "EFBToRAMEnable"))
		iniFile.Get("Video", "EFBToRAMEnable", &bCopyEFBToRAM, 0);
	if (iniFile.Exists("Video", "SafeTextureCache"))
		iniFile.Get("Video", "SafeTextureCache", &bSafeTextureCache, false);	
	if (iniFile.Exists("Video", "MSAA"))
		iniFile.Get("Video", "MSAA", &iMultisampleMode, 0);
	if (iniFile.Exists("Video", "DstAlphaPass"))
		iniFile.Get("Video", "DstAlphaPass", &bDstAlphaPass, false);
	if (iniFile.Exists("Video", "UseXFB"))
		iniFile.Get("Video", "UseXFB", &bUseXFB, 0);
	if (iniFile.Exists("Video", "ProjectionHack"))
		iniFile.Get("Video", "ProjectionHack", &iPhackvalue, 0);
}

void VideoConfig::Save(const char *ini_file)
{
    IniFile iniFile;
    iniFile.Load(ini_file);
    iniFile.Set("Hardware", "WindowedRes", cInternalRes);
    iniFile.Set("Hardware", "FullscreenRes", cFSResolution);
    iniFile.Set("Hardware", "Fullscreen", bFullscreen);
    iniFile.Set("Hardware", "VSync", bVSync);
    iniFile.Set("Hardware", "RenderToMainframe", RenderToMainframe);
    iniFile.Set("Settings", "StretchToFit", bNativeResolution);
	iniFile.Set("Settings", "2xResolution", b2xResolution);
    iniFile.Set("Settings", "KeepAR_4_3", bKeepAR43);
	iniFile.Set("Settings", "KeepAR_16_9", bKeepAR169);
	iniFile.Set("Settings", "Crop", bCrop);
	iniFile.Set("Settings", "wideScreenHack", bWidescreenHack);
    iniFile.Set("Settings", "HideCursor", bHideCursor);
    iniFile.Set("Settings", "UseXFB", bUseXFB);
    iniFile.Set("Settings", "AutoScale", bAutoScale);

    iniFile.Set("Settings", "SafeTextureCache", bSafeTextureCache);
    iniFile.Set("Settings", "ShowFPS", bShowFPS);
    iniFile.Set("Settings", "OverlayStats", bOverlayStats);
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
	iniFile.Set("Enhancements", "PostProcessingShader", sPostProcessingShader);
    
    iniFile.Set("Hacks", "EFBAccessEnable", bEFBAccessEnable);
    iniFile.Set("Hacks", "EFBCopyDisable", bEFBCopyDisable);
    iniFile.Set("Hacks", "EFBCopyDisableHotKey", bOSDHotKey);
	iniFile.Set("Hacks", "EFBToTextureEnable", bCopyEFBToRAM);
	iniFile.Set("Hacks", "ProjectionHack", iPhackvalue);

	iniFile.Set("Hardware", "Adapter", iAdapter);
	iniFile.Set("Hardware", "SimpleFB", bSimpleFB);

    iniFile.Save(ini_file);
}

// TODO: Figure out a better place for this function.
void ComputeDrawRectangle(int backbuffer_width, int backbuffer_height, bool flip, TargetRectangle *rc)
{
	float FloatGLWidth = (float)backbuffer_width;
	float FloatGLHeight = (float)backbuffer_height;
	float FloatXOffset = 0;
	float FloatYOffset = 0;

	// The rendering window size
	const float WinWidth = FloatGLWidth;
	const float WinHeight = FloatGLHeight;

	// Handle aspect ratio.
	if (g_ActiveConfig.bKeepAR43 || g_ActiveConfig.bKeepAR169)
	{
		// The rendering window aspect ratio as a proportion of the 4:3 or 16:9 ratio
		float Ratio = (WinWidth / WinHeight) / (g_Config.bKeepAR43 ? (4.0f / 3.0f) : (16.0f / 9.0f));
		// Check if height or width is the limiting factor. If ratio > 1 the picture is too wide and have to limit the width.
		if (Ratio > 1.0f)
		{
			// Scale down and center in the X direction.
			FloatGLWidth /= Ratio;
			FloatXOffset = (WinWidth - FloatGLWidth) / 2.0f;
		}
		// The window is too high, we have to limit the height
		else
		{
			// Scale down and center in the Y direction.
			FloatGLHeight *= Ratio;
			FloatYOffset = FloatYOffset + (WinHeight - FloatGLHeight) / 2.0f;
		}
	}

	// -----------------------------------------------------------------------
	// Crop the picture from 4:3 to 5:4 or from 16:9 to 16:10.
	//		Output: FloatGLWidth, FloatGLHeight, FloatXOffset, FloatYOffset
	// ------------------
	if ((g_ActiveConfig.bKeepAR43 || g_ActiveConfig.bKeepAR169) && g_ActiveConfig.bCrop)
	{
		float Ratio = g_Config.bKeepAR43 ? ((4.0f / 3.0f) / (5.0f / 4.0f)) : (((16.0f / 9.0f) / (16.0f / 10.0f)));
		// The width and height we will add (calculate this before FloatGLWidth and FloatGLHeight is adjusted)
		float IncreasedWidth = (Ratio - 1.0f) * FloatGLWidth;
		float IncreasedHeight = (Ratio - 1.0f) * FloatGLHeight;
		// The new width and height
		FloatGLWidth = FloatGLWidth * Ratio;
		FloatGLHeight = FloatGLHeight * Ratio;
		// Adjust the X and Y offset
		FloatXOffset = FloatXOffset - (IncreasedWidth * 0.5f);
		FloatYOffset = FloatYOffset - (IncreasedHeight * 0.5f);
	}

	int XOffset = (int)(FloatXOffset + 0.5f);
	int YOffset = (int)(FloatYOffset + 0.5f);
	rc->left = XOffset;
	rc->top = flip ? (int)(YOffset + ceil(FloatGLHeight)) : YOffset;
	rc->right = XOffset + (int)ceil(FloatGLWidth);
	rc->bottom = flip ? YOffset : (int)(YOffset + ceil(FloatGLHeight));
}
