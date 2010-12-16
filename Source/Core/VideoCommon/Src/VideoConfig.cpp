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

	// Needed for the first frame, I think
	fAspectRatioHackW = 1;
	fAspectRatioHackH = 1;

	// disable all features by default
	backend_info.APIType = API_NONE;
	backend_info.bAllowSignedBytes = false;
	backend_info.bUseRGBATextures = false;
	backend_info.bSupportsEFBToRAM = false;
	backend_info.bSupportsRealXFB = false;
	backend_info.bSupports3DVision = false;
}

void VideoConfig::Load(const char *ini_file)
{
	std::string temp;
	IniFile iniFile;
	iniFile.Load(ini_file);
	
	iniFile.Get("Hardware", "VSync", &bVSync, 0); // Hardware
	iniFile.Get("Settings", "wideScreenHack", &bWidescreenHack, false);
	iniFile.Get("Settings", "AspectRatio", &iAspectRatio, (int)ASPECT_AUTO);
	iniFile.Get("Settings", "Crop", &bCrop, false);
	iniFile.Get("Settings", "UseXFB", &bUseXFB, 0);
	iniFile.Get("Settings", "UseRealXFB", &bUseRealXFB, 0);
	iniFile.Get("Settings", "UseNativeMips", &bUseNativeMips, true);
	
	iniFile.Get("Settings", "SafeTextureCache", &bSafeTextureCache, false); // Settings
	//Safe texture cache params
	iniFile.Get("Settings", "SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples,512);		
	
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
	iniFile.Get("Settings", "AnaglyphStereo", &bAnaglyphStereo, false);
	iniFile.Get("Settings", "AnaglyphStereoSeparation", &iAnaglyphStereoSeparation, 200);
	iniFile.Get("Settings", "AnaglyphFocalAngle", &iAnaglyphFocalAngle, 0);
	iniFile.Get("Settings", "EnablePixelLigting", &bEnablePixelLigting, 0);
	
	iniFile.Get("Settings", "ShowShaderErrors", &bShowShaderErrors, 0);
	iniFile.Get("Settings", "MSAA", &iMultisampleMode, 0);
	iniFile.Get("Settings", "EFBScale", &iEFBScale, 0);
	
	iniFile.Get("Settings", "DstAlphaPass", &bDstAlphaPass, false);
	
	iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
	iniFile.Get("Settings", "WireFrame", &bWireFrame, 0);
	iniFile.Get("Settings", "DisableLighting", &bDisableLighting, 0);
	iniFile.Get("Settings", "DisableTexturing", &bDisableTexturing, 0);
	iniFile.Get("Settings", "DisableFog", &bDisableFog, 0);
	
	iniFile.Get("Settings", "EnableOpenCL", &bEnableOpenCL, false);

	iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
	iniFile.Get("Enhancements", "MaxAnisotropy", &iMaxAnisotropy, 1);  // NOTE - this is x in (1 << x)
	iniFile.Get("Enhancements", "PostProcessingShader", &sPostProcessingShader, "");
	iniFile.Get("Enhancements", "Enable3dVision", &b3DVision, false);
	
	iniFile.Get("Hacks", "EFBAccessEnable", &bEFBAccessEnable, true);
	iniFile.Get("Hacks", "DlistCachingEnable", &bDlistCachingEnable,false);
	iniFile.Get("Hacks", "EFBCopyEnable", &bEFBCopyEnable, true);
	iniFile.Get("Hacks", "EFBCopyDisableHotKey", &bOSDHotKey, 0);
	iniFile.Get("Hacks", "EFBToTextureEnable", &bCopyEFBToTexture, false);
	iniFile.Get("Hacks", "EFBScaledCopy", &bCopyEFBScaled, true);
	iniFile.Get("Hacks", "ProjectionHack", &iPhackvalue, 0);

	iniFile.Get("Hardware", "Adapter", &iAdapter, 0);
	if (iAdapter == -1)
		iAdapter = 0;
	
	// Load common settings
	iniFile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	bool bTmp;
	iniFile.Get("Interface", "UsePanicHandlers", &bTmp, true);
	SetEnableAlert(bTmp);
}

void VideoConfig::GameIniLoad(const char *ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);
	
	if (iniFile.Exists("Video", "ForceFiltering"))
		iniFile.Get("Video", "ForceFiltering", &bForceFiltering);
	if (iniFile.Exists("Video", "MaxAnisotropy"))
		iniFile.Get("Video", "MaxAnisotropy", &iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	if (iniFile.Exists("Video", "EFBCopyEnable"))
		iniFile.Get("Video", "EFBCopyEnable", &bEFBCopyEnable);
	if (iniFile.Exists("Video", "EFBCopyDisableHotKey"))
		iniFile.Get("Video", "EFBCopyDisableHotKey", &bOSDHotKey);
	if (iniFile.Exists("Video", "EFBToTextureEnable"))
		iniFile.Get("Video", "EFBToTextureEnable", &bCopyEFBToTexture);	
	if (iniFile.Exists("Video", "EFBScaledCopy"))
		iniFile.Get("Video", "EFBScaledCopy", &bCopyEFBScaled);
	if (iniFile.Exists("Video", "SafeTextureCache"))
		iniFile.Get("Video", "SafeTextureCache", &bSafeTextureCache);
	//Safe texture cache params
	if (iniFile.Exists("Video", "SafeTextureCacheColorSamples"))
		iniFile.Get("Video", "SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples);

	if (iniFile.Exists("Video", "MSAA"))
		iniFile.Get("Video", "MSAA", &iMultisampleMode);
	if (iniFile.Exists("Video", "EFBScale"))
		iniFile.Get("Video", "EFBScale", &iEFBScale, 0);
	if (iniFile.Exists("Video", "DstAlphaPass"))
		iniFile.Get("Video", "DstAlphaPass", &bDstAlphaPass);
	if (iniFile.Exists("Video", "UseXFB"))
		iniFile.Get("Video", "UseXFB", &bUseXFB);
	if (iniFile.Exists("Video", "UseRealXFB"))
		iniFile.Get("Video", "UseRealXFB", &bUseRealXFB);
	if (iniFile.Exists("Video", "ProjectionHack"))
		iniFile.Get("Video", "ProjectionHack", &iPhackvalue);
	if (iniFile.Exists("Video", "UseNativeMips"))
		iniFile.Get("Video", "UseNativeMips", &bUseNativeMips);
	if (iniFile.Exists("Video", "ZTPSpeedupHack"))
	   iniFile.Get("Video", "ZTPSpeedupHack", &bZTPSpeedHack);
	if (iniFile.Exists("Video", "DlistCachingEnable"))
	   iniFile.Get("Video", "DlistCachingEnable", &bDlistCachingEnable);
}

void VideoConfig::Save(const char *ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);
	iniFile.Set("Hardware", "VSync", bVSync);
	iniFile.Set("Settings", "AspectRatio", iAspectRatio);
	iniFile.Set("Settings", "Crop", bCrop);
	iniFile.Set("Settings", "wideScreenHack", bWidescreenHack);
	iniFile.Set("Settings", "UseXFB", bUseXFB);
	iniFile.Set("Settings", "UseRealXFB", bUseRealXFB);
	iniFile.Set("Settings", "UseNativeMips", bUseNativeMips);

	iniFile.Set("Settings", "SafeTextureCache", bSafeTextureCache);
	//safe texture cache params
	iniFile.Set("Settings", "SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);

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
	iniFile.Set("Settings", "AnaglyphStereo", bAnaglyphStereo);
	iniFile.Set("Settings", "AnaglyphStereoSeparation", iAnaglyphStereoSeparation);
	iniFile.Set("Settings", "AnaglyphFocalAngle", iAnaglyphFocalAngle);
	iniFile.Set("Settings", "EnablePixelLigting", bEnablePixelLigting);

	iniFile.Set("Settings", "ShowEFBCopyRegions", bShowEFBCopyRegions);
	iniFile.Set("Settings", "ShowShaderErrors", bShowShaderErrors);
	iniFile.Set("Settings", "MSAA", iMultisampleMode);
	iniFile.Set("Settings", "EFBScale", iEFBScale);
	iniFile.Set("Settings", "TexFmtOverlayEnable", bTexFmtOverlayEnable);
	iniFile.Set("Settings", "TexFmtOverlayCenter", bTexFmtOverlayCenter);
	iniFile.Set("Settings", "Wireframe", bWireFrame);
	iniFile.Set("Settings", "DisableLighting", bDisableLighting);
	iniFile.Set("Settings", "DisableTexturing", bDisableTexturing);
	iniFile.Set("Settings", "DstAlphaPass", bDstAlphaPass);
	iniFile.Set("Settings", "DisableFog", bDisableFog);

	iniFile.Set("Settings", "EnableOpenCL", bEnableOpenCL);
	
	iniFile.Set("Enhancements", "ForceFiltering", bForceFiltering);
	iniFile.Set("Enhancements", "MaxAnisotropy", iMaxAnisotropy);
	iniFile.Set("Enhancements", "PostProcessingShader", sPostProcessingShader);
	iniFile.Set("Enhancements", "Enable3dVision", b3DVision);
	
	iniFile.Set("Hacks", "EFBAccessEnable", bEFBAccessEnable);
	iniFile.Set("Hacks", "DlistCachingEnable", bDlistCachingEnable);
	iniFile.Set("Hacks", "EFBCopyEnable", bEFBCopyEnable);
	iniFile.Set("Hacks", "EFBCopyDisableHotKey", bOSDHotKey);
	iniFile.Set("Hacks", "EFBToTextureEnable", bCopyEFBToTexture);	
	iniFile.Set("Hacks", "EFBScaledCopy", bCopyEFBScaled);
	iniFile.Set("Hacks", "ProjectionHack", iPhackvalue);
	
	iniFile.Set("Hardware", "Adapter", iAdapter);
	
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
