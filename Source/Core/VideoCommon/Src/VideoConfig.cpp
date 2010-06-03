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
#include "FileUtil.h"

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

	// Needed for the first frame, I think
	fAspectRatioHackW = 1;
	fAspectRatioHackH = 1;
}

void VideoConfig::Load(const char *ini_file)
{
    std::string temp;
    IniFile iniFile;
    iniFile.Load(ini_file);

	Section& hardware = iniFile["Hardware"];
    hardware.Get("VSync", &bVSync, false); // Hardware

	Section& settings = iniFile["Settings"];
    settings.Get("StretchToFit", &bNativeResolution, true);
	settings.Get("2xResolution", &b2xResolution, false);
	settings.Get("wideScreenHack", &bWidescreenHack, false);
	settings.Get("AspectRatio", &iAspectRatio, (int)ASPECT_AUTO);
	settings.Get("Crop", &bCrop, false);
    settings.Get("UseXFB", &bUseXFB, true);
	settings.Get("UseRealXFB", &bUseRealXFB, false);
    settings.Get("AutoScale", &bAutoScale, true);
	settings.Get("UseNativeMips", &bUseNativeMips, true);
    
    settings.Get("SafeTextureCache", &bSafeTextureCache, false); // Settings
	//Safe texture cache params
	settings.Get("SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples,512);		

    settings.Get("ShowFPS", &bShowFPS, false); // Settings
    settings.Get("OverlayStats", &bOverlayStats, false);
	settings.Get("OverlayProjStats", &bOverlayProjStats, false);
	settings.Get("ShowEFBCopyRegions", &bShowEFBCopyRegions, false);
    settings.Get("DLOptimize", &iCompileDLsLevel, 0);
    settings.Get("DumpTextures", &bDumpTextures, false);
    settings.Get("HiresTextures", &bHiresTextures, false);
	settings.Get("DumpEFBTarget", &bDumpEFBTarget, false);
	settings.Get("DumpFrames", &bDumpFrames, false);
    settings.Get("FreeLook", &bFreeLook, false);
    settings.Get("ShowShaderErrors", &bShowShaderErrors, false);
    settings.Get("MSAA", &iMultisampleMode, 0);
    settings.Get("DstAlphaPass", &bDstAlphaPass, false);
    
    settings.Get("TexFmtOverlayEnable", &bTexFmtOverlayEnable, false);
    settings.Get("TexFmtOverlayCenter", &bTexFmtOverlayCenter, false);
    settings.Get("WireFrame", &bWireFrame, false);
    settings.Get("DisableLighting", &bDisableLighting, false);
    settings.Get("DisableTexturing", &bDisableTexturing, false);
	settings.Get("DisableFog", &bDisableFog, false);

	Section& enhancements = iniFile["Enhancements"];
    enhancements.Get("ForceFiltering", &bForceFiltering, false);
    enhancements.Get("MaxAnisotropy", &iMaxAnisotropy, 1);  // NOTE - this is x in (1 << x)
	enhancements.Get("PostProcessingShader", &sPostProcessingShader, "");

	Section& hacks = iniFile["Hacks"];
    hacks.Get("EFBAccessEnable", &bEFBAccessEnable, true);
    hacks.Get("EFBCopyDisable", &bEFBCopyDisable, false);
    hacks.Get("EFBCopyDisableHotKey", &bOSDHotKey, false);
	hacks.Get("EFBToTextureEnable", &bCopyEFBToTexture, false);
	hacks.Get("EFBScaledCopy", &bCopyEFBScaled, true);
	hacks.Get("FIFOBPHack", &bFIFOBPhack, false);
	hacks.Get("ProjectionHack", &iPhackvalue, 0);

	hardware.Get("Adapter", &iAdapter, 0);
	if (iAdapter == -1) 
		iAdapter = 0;
	hardware.Get("SimpleFB", &bSimpleFB, false);

	// Load common settings
	iniFile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	bool bTmp;
	iniFile["Interface"].Get("UsePanicHandlers", &bTmp, true);
	SetEnableAlert(bTmp);
}

void VideoConfig::GameIniLoad(const char *ini_file)
{
    IniFile iniFile;
    iniFile.Load(ini_file);

	Section& video = iniFile["Video"];

	video.Get("ForceFiltering", &bForceFiltering, bForceFiltering);
	video.Get("MaxAnisotropy", &iMaxAnisotropy, iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	video.Get("EFBCopyDisable", &bEFBCopyDisable, bEFBCopyDisable);
	video.Get("EFBCopyDisableHotKey", &bOSDHotKey, bOSDHotKey);
	video.Get("EFBToTextureEnable", &bCopyEFBToTexture, bCopyEFBToTexture);
	video.Get("EFBScaledCopy", &bCopyEFBScaled, bCopyEFBScaled);
	video.Get("SafeTextureCache", &bSafeTextureCache, bSafeTextureCache);	
	//Safe texture cache params
	video.Get("SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples, iSafeTextureCache_ColorSamples);	

	video.Get("MSAA", &iMultisampleMode, iMultisampleMode);
	video.Get("DstAlphaPass", &bDstAlphaPass, bDstAlphaPass);
	video.Get("UseXFB", &bUseXFB, bUseXFB);
	video.Get("UseRealXFB", &bUseRealXFB, bUseRealXFB);
	video.Get("FIFOBPHack", &bFIFOBPhack, bFIFOBPhack);
	video.Get("ProjectionHack", &iPhackvalue, iPhackvalue);
	video.Get("UseNativeMips", &bUseNativeMips, bUseNativeMips);
}

void VideoConfig::Save(const char *ini_file)
{
    IniFile iniFile;
    iniFile.Load(ini_file);

	Section& hardware = iniFile["Hardware"];
    hardware.Set("VSync", bVSync);

	Section& settings = iniFile["Settings"];
    settings.Set("StretchToFit", bNativeResolution);
	settings.Set("2xResolution", b2xResolution);
	settings.Set("AspectRatio", iAspectRatio);
	settings.Set("Crop", bCrop);
	settings.Set("wideScreenHack", bWidescreenHack);
    settings.Set("UseXFB", bUseXFB);
	settings.Set("UseRealXFB", bUseRealXFB);
    settings.Set("AutoScale", bAutoScale);
	settings.Set("UseNativeMips", bUseNativeMips);

    settings.Set("SafeTextureCache", bSafeTextureCache);
	//safe texture cache params
	settings.Set("SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);

    settings.Set("ShowFPS", bShowFPS);
    settings.Set("OverlayStats", bOverlayStats);
	settings.Set("OverlayProjStats", bOverlayProjStats);
    settings.Set("DLOptimize", iCompileDLsLevel);
	settings.Set("Show", iCompileDLsLevel);
    settings.Set("DumpTextures", bDumpTextures);
    settings.Set("HiresTextures", bHiresTextures);
	settings.Set("DumpEFBTarget", bDumpEFBTarget);
	settings.Set("DumpFrames", bDumpFrames);
    settings.Set("FreeLook", bFreeLook);
    settings.Set("ShowEFBCopyRegions", bShowEFBCopyRegions);
	settings.Set("ShowShaderErrors", bShowShaderErrors);
    settings.Set("MSAA", iMultisampleMode);
    settings.Set("TexFmtOverlayEnable", bTexFmtOverlayEnable);
    settings.Set("TexFmtOverlayCenter", bTexFmtOverlayCenter);
    settings.Set("Wireframe", bWireFrame);
    settings.Set("DisableLighting", bDisableLighting);
    settings.Set("DisableTexturing", bDisableTexturing);
    settings.Set("DstAlphaPass", bDstAlphaPass);
	settings.Set("DisableFog", bDisableFog);
    
	Section& enhancements = iniFile["Enhancements"];
    enhancements.Set("ForceFiltering", bForceFiltering);
    enhancements.Set("MaxAnisotropy", iMaxAnisotropy);
	enhancements.Set("PostProcessingShader", sPostProcessingShader);
    
	Section& hacks = iniFile["Hacks"];
    hacks.Set("EFBAccessEnable", bEFBAccessEnable);
    hacks.Set("EFBCopyDisable", bEFBCopyDisable);
    hacks.Set("EFBCopyDisableHotKey", bOSDHotKey);
	hacks.Set("EFBToTextureEnable", bCopyEFBToTexture);
	hacks.Set("EFBScaledCopy", bCopyEFBScaled);
	hacks.Set("FIFOBPHack", bFIFOBPhack);
	hacks.Set("ProjectionHack", iPhackvalue);

	hardware.Set("Adapter", iAdapter);
	hardware.Set("SimpleFB", bSimpleFB);

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
	// Default to auto.
	bool use16_9 = g_VideoInitialize.bAutoAspectIs16_9;

	// Update aspect ratio hack values
	// Won't take effect until next frame
	// Don't know if there is a better place for this code so there isn't a 1 frame delay
	if ( g_ActiveConfig.bWidescreenHack )
	{
		float source_aspect = use16_9 ? (16.0f / 9.0f) : (4.0f / 3.0f);
		float target_aspect;

		switch ( g_ActiveConfig.iAspectRatio )
		{
		case ASPECT_FORCE_16_9 :
			target_aspect = 16.0f / 9.0f;
			break;
		case ASPECT_FORCE_4_3 :
			target_aspect = 4.0f / 3.0f;
			break;
		case ASPECT_STRETCH :
			target_aspect = WinWidth / WinHeight;
			break;
		default :
			// ASPECT_AUTO == no hacking
			target_aspect = source_aspect;
			break;
		}

		float adjust = source_aspect / target_aspect;
		if ( adjust > 1 )
		{
			// Vert+
			g_Config.fAspectRatioHackW = 1;
			g_Config.fAspectRatioHackH = 1/adjust;
		}
		else
		{
			// Hor+
			g_Config.fAspectRatioHackW = adjust;
			g_Config.fAspectRatioHackH = 1;
		}
	}
	else
	{
		// Hack is disabled
		g_Config.fAspectRatioHackW = 1;
		g_Config.fAspectRatioHackH = 1;
	}
	
	// Check for force-settings and override.
	if (g_ActiveConfig.iAspectRatio == ASPECT_FORCE_16_9)
		use16_9 = true;
	else if (g_ActiveConfig.iAspectRatio == ASPECT_FORCE_4_3)
		use16_9 = false;

	if (g_ActiveConfig.iAspectRatio != ASPECT_STRETCH)
	{
		// The rendering window aspect ratio as a proportion of the 4:3 or 16:9 ratio
		float Ratio = (WinWidth / WinHeight) / (!use16_9 ? (4.0f / 3.0f) : (16.0f / 9.0f));
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
	if (g_ActiveConfig.iAspectRatio != ASPECT_STRETCH && g_ActiveConfig.bCrop)
	{
		float Ratio = !use16_9 ? ((4.0f / 3.0f) / (5.0f / 4.0f)) : (((16.0f / 9.0f) / (16.0f / 10.0f)));
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
	int iWhidth = (int)ceil(FloatGLWidth);
	int iHeight = (int)ceil(FloatGLHeight);
	iWhidth -= iWhidth % 4; // ensure divisibility by 4 to make it compatible with all the video encoders
	iHeight -= iHeight % 4;
	rc->left = XOffset;
	rc->top = flip ? (int)(YOffset + iHeight) : YOffset;
	rc->right = XOffset + iWhidth;
	rc->bottom = flip ? YOffset : (int)(YOffset + iHeight);
}
