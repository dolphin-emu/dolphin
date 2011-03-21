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
	backend_info.bUseRGBATextures = false;
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
	iniFile.Get("Settings", "ShowInputDisplay", &bShowInputDisplay, false);
	iniFile.Get("Settings", "OverlayStats", &bOverlayStats, false);
	iniFile.Get("Settings", "OverlayProjStats", &bOverlayProjStats, false);
	iniFile.Get("Settings", "ShowEFBCopyRegions", &bShowEFBCopyRegions, false);
	iniFile.Get("Settings", "DLOptimize", &iCompileDLsLevel, 0);
	iniFile.Get("Settings", "DumpTextures", &bDumpTextures, 0);
	iniFile.Get("Settings", "HiresTextures", &bHiresTextures, 0);
	iniFile.Get("Settings", "DumpEFBTarget", &bDumpEFBTarget, 0);
	iniFile.Get("Settings", "DumpFrames", &bDumpFrames, 0);
	iniFile.Get("Settings", "FreeLook", &bFreeLook, 0);
	iniFile.Get("Settings", "UseFFV1", &bUseFFV1, 0);
	iniFile.Get("Settings", "AnaglyphStereo", &bAnaglyphStereo, false);
	iniFile.Get("Settings", "AnaglyphStereoSeparation", &iAnaglyphStereoSeparation, 200);
	iniFile.Get("Settings", "AnaglyphFocalAngle", &iAnaglyphFocalAngle, 0);
	iniFile.Get("Settings", "EnablePixelLighting", &bEnablePixelLighting, 0);
	iniFile.Get("Settings", "EnablePerPixelDepth", &bEnablePerPixelDepth, 0);
	
	iniFile.Get("Settings", "ShowShaderErrors", &bShowShaderErrors, 0);
	iniFile.Get("Settings", "MSAA", &iMultisampleMode, 0);
	iniFile.Get("Settings", "EFBScale", &iEFBScale, 1); // integral
	
	iniFile.Get("Settings", "DstAlphaPass", &bDstAlphaPass, false);
	
	iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
	iniFile.Get("Settings", "WireFrame", &bWireFrame, 0);
	iniFile.Get("Settings", "DisableLighting", &bDisableLighting, 0);
	iniFile.Get("Settings", "DisableTexturing", &bDisableTexturing, 0);
	iniFile.Get("Settings", "DisableFog", &bDisableFog, 0);
	
	iniFile.Get("Settings", "EnableOpenCL", &bEnableOpenCL, false);
#ifdef _OPENMP
	iniFile.Get("Settings", "OMPDecoder", &bOMPDecoder, false);
#endif

	iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
	iniFile.Get("Enhancements", "MaxAnisotropy", &iMaxAnisotropy, 0);  // NOTE - this is x in (1 << x)
	iniFile.Get("Enhancements", "PostProcessingShader", &sPostProcessingShader, "");
	iniFile.Get("Enhancements", "Enable3dVision", &b3DVision, false);
	
	iniFile.Get("Hacks", "EFBAccessEnable", &bEFBAccessEnable, true);
	iniFile.Get("Hacks", "DlistCachingEnable", &bDlistCachingEnable,false);
	iniFile.Get("Hacks", "EFBCopyEnable", &bEFBCopyEnable, true);
	iniFile.Get("Hacks", "EFBCopyDisableHotKey", &bOSDHotKey, 0);
	iniFile.Get("Hacks", "EFBToTextureEnable", &bCopyEFBToTexture, false);
	iniFile.Get("Hacks", "EFBScaledCopy", &bCopyEFBScaled, true);
	iniFile.Get("Hacks", "EFBCopyCacheEnable", &bEFBCopyCacheEnable, false);
	iniFile.Get("Hacks", "EFBEmulateFormatChanges", &bEFBEmulateFormatChanges, true);

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

	iniFile.GetIfExists("Video_Hardware", "VSync", &bVSync);
	iniFile.GetIfExists("Video_Settings", "wideScreenHack", &bWidescreenHack);
	iniFile.GetIfExists("Video_Settings", "AspectRatio", &iAspectRatio);
	iniFile.GetIfExists("Video_Settings", "Crop", &bCrop);
	iniFile.GetIfExists("Video_Settings", "UseXFB", &bUseXFB);
	iniFile.GetIfExists("Video_Settings", "UseRealXFB", &bUseRealXFB);
	iniFile.GetIfExists("Video_Settings", "UseNativeMips", &bUseNativeMips);

	iniFile.GetIfExists("Video_Settings", "SafeTextureCache", &bSafeTextureCache);
	iniFile.GetIfExists("Video_Settings", "SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples);

	iniFile.GetIfExists("Video_Settings", "ShowFPS", &bShowFPS);
	iniFile.GetIfExists("Video_Settings", "ShowInputDisplay", &bShowInputDisplay);
	iniFile.GetIfExists("Video_Settings", "OverlayStats", &bOverlayStats);
	iniFile.GetIfExists("Video_Settings", "OverlayProjStats", &bOverlayProjStats);
	iniFile.GetIfExists("Video_Settings", "ShowEFBCopyRegions", &bShowEFBCopyRegions);
	iniFile.GetIfExists("Video_Settings", "DLOptimize", &iCompileDLsLevel);
	iniFile.GetIfExists("Video_Settings", "DumpTextures", &bDumpTextures);
	iniFile.GetIfExists("Video_Settings", "HiresTextures", &bHiresTextures);
	iniFile.GetIfExists("Video_Settings", "DumpEFBTarget", &bDumpEFBTarget);
	iniFile.GetIfExists("Video_Settings", "DumpFrames", &bDumpFrames);
	iniFile.GetIfExists("Video_Settings", "FreeLook", &bFreeLook);
	iniFile.GetIfExists("Video_Settings", "UseFFV1", &bUseFFV1);
	iniFile.GetIfExists("Video_Settings", "AnaglyphStereo", &bAnaglyphStereo);
	iniFile.GetIfExists("Video_Settings", "AnaglyphStereoSeparation", &iAnaglyphStereoSeparation);
	iniFile.GetIfExists("Video_Settings", "AnaglyphFocalAngle", &iAnaglyphFocalAngle);
	iniFile.GetIfExists("Video_Settings", "EnablePixelLighting", &bEnablePixelLighting);
	iniFile.GetIfExists("Video_Settings", "EnablePerPixelDepth", &bEnablePerPixelDepth);

	iniFile.GetIfExists("Video_Settings", "ShowShaderErrors", &bShowShaderErrors);
	iniFile.GetIfExists("Video_Settings", "MSAA", &iMultisampleMode);
	iniFile.GetIfExists("Video_Settings", "EFBScale", &iEFBScale); // integral

	iniFile.GetIfExists("Video_Settings", "DstAlphaPass", &bDstAlphaPass);

	iniFile.GetIfExists("Video_Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable);
	iniFile.GetIfExists("Video_Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter);
	iniFile.GetIfExists("Video_Settings", "WireFrame", &bWireFrame);
	iniFile.GetIfExists("Video_Settings", "DisableLighting", &bDisableLighting);
	iniFile.GetIfExists("Video_Settings", "DisableTexturing", &bDisableTexturing);
	iniFile.GetIfExists("Video_Settings", "DisableFog", &bDisableFog);

	iniFile.GetIfExists("Video_Settings", "EnableOpenCL", &bEnableOpenCL);
#ifdef _OPENMP
	iniFile.GetIfExists("Video_Settings", "OMPDecoder", &bOMPDecoder);
#endif

	iniFile.GetIfExists("Video_Enhancements", "ForceFiltering", &bForceFiltering);
	iniFile.GetIfExists("Video_Enhancements", "MaxAnisotropy", &iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	iniFile.GetIfExists("Video_Enhancements", "PostProcessingShader", &sPostProcessingShader);
	iniFile.GetIfExists("Video_Enhancements", "Enable3dVision", &b3DVision);

	iniFile.GetIfExists("Video_Hacks", "EFBAccessEnable", &bEFBAccessEnable);
	iniFile.GetIfExists("Video_Hacks", "DlistCachingEnable", &bDlistCachingEnable);
	iniFile.GetIfExists("Video_Hacks", "EFBCopyEnable", &bEFBCopyEnable);
	iniFile.GetIfExists("Video_Hacks", "EFBCopyDisableHotKey", &bOSDHotKey);
	iniFile.GetIfExists("Video_Hacks", "EFBToTextureEnable", &bCopyEFBToTexture);
	iniFile.GetIfExists("Video_Hacks", "EFBScaledCopy", &bCopyEFBScaled);
	iniFile.GetIfExists("Video_Hacks", "EFBCopyCacheEnable", &bEFBCopyCacheEnable);
	iniFile.GetIfExists("Video_Hacks", "EFBEmulateFormatChanges", &bEFBEmulateFormatChanges);

	iniFile.GetIfExists("Video_Hardware", "Adapter", &iAdapter);


	iniFile.GetIfExists("Video", "ForceFiltering", &bForceFiltering);
	iniFile.GetIfExists("Video", "MaxAnisotropy", &iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	iniFile.GetIfExists("Video", "EFBCopyEnable", &bEFBCopyEnable);
	iniFile.GetIfExists("Video", "EFBCopyDisableHotKey", &bOSDHotKey);
	iniFile.GetIfExists("Video", "EFBAccessEnable", &bEFBAccessEnable);
	iniFile.GetIfExists("Video", "EFBToTextureEnable", &bCopyEFBToTexture);	
	iniFile.GetIfExists("Video", "EFBScaledCopy", &bCopyEFBScaled);
	iniFile.GetIfExists("Video", "SafeTextureCache", &bSafeTextureCache);
	iniFile.GetIfExists("Video", "SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples);

	iniFile.GetIfExists("Video", "MSAA", &iMultisampleMode);
	iniFile.GetIfExists("Video", "EFBScale", &iEFBScale);
	iniFile.GetIfExists("Video", "DstAlphaPass", &bDstAlphaPass);
	iniFile.GetIfExists("Video", "UseXFB", &bUseXFB);
	iniFile.GetIfExists("Video", "UseRealXFB", &bUseRealXFB);
	iniFile.GetIfExists("Video", "ProjectionHack", &iPhackvalue[0]);
	iniFile.GetIfExists("Video", "PH_SZNear", &iPhackvalue[1]);
	iniFile.GetIfExists("Video", "PH_SZFar", &iPhackvalue[2]);
	iniFile.GetIfExists("Video", "PH_ExtraParam", &iPhackvalue[3]);
	iniFile.GetIfExists("Video", "PH_ZNear", &sPhackvalue[0]);
	iniFile.GetIfExists("Video", "PH_ZFar", &sPhackvalue[1]);
	iniFile.GetIfExists("Video", "UseNativeMips", &bUseNativeMips);
	iniFile.GetIfExists("Video", "ZTPSpeedupHack", &bZTPSpeedHack);
	iniFile.GetIfExists("Video", "DlistCachingEnable", &bDlistCachingEnable);
}

void VideoConfig::VerifyValidity()
{
	// TODO: Check iMaxAnisotropy value
	if (iAdapter > ((int)backend_info.Adapters.size() - 1)) iAdapter = 0;
	if (iMultisampleMode < 0 || iMultisampleMode >= (int)backend_info.AAModes.size()) iMultisampleMode = 0;
	if (!backend_info.bSupports3DVision) b3DVision = false;
	if (!backend_info.bSupportsFormatReinterpretation) bEFBEmulateFormatChanges = false;
	if (!backend_info.bSupportsPixelLighting) bEnablePixelLighting = false;
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
	iniFile.Set("Settings", "ShowInputDisplay", bShowInputDisplay);
	iniFile.Set("Settings", "OverlayStats", bOverlayStats);
	iniFile.Set("Settings", "OverlayProjStats", bOverlayProjStats);
	iniFile.Set("Settings", "DLOptimize", iCompileDLsLevel);
	iniFile.Set("Settings", "Show", iCompileDLsLevel);
	iniFile.Set("Settings", "DumpTextures", bDumpTextures);
	iniFile.Set("Settings", "HiresTextures", bHiresTextures);
	iniFile.Set("Settings", "DumpEFBTarget", bDumpEFBTarget);
	iniFile.Set("Settings", "DumpFrames", bDumpFrames);
	iniFile.Set("Settings", "FreeLook", bFreeLook);
	iniFile.Set("Settings", "UseFFV1", bUseFFV1);
	iniFile.Set("Settings", "AnaglyphStereo", bAnaglyphStereo);
	iniFile.Set("Settings", "AnaglyphStereoSeparation", iAnaglyphStereoSeparation);
	iniFile.Set("Settings", "AnaglyphFocalAngle", iAnaglyphFocalAngle);
	iniFile.Set("Settings", "EnablePixelLighting", bEnablePixelLighting);
	iniFile.Set("Settings", "EnablePerPixelDepth", bEnablePerPixelDepth);
	

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
#ifdef _OPENMP
	iniFile.Set("Settings", "OMPDecoder", bOMPDecoder);
#endif
	
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
	iniFile.Set("Hacks", "EFBCopyCacheEnable", bEFBCopyCacheEnable);
	iniFile.Set("Hacks", "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

	iniFile.Set("Hardware", "Adapter", iAdapter);
	
	iniFile.Save(ini_file);
}

// haxhaxhax
static void SetUndetermined(bool& val)
{
	// lul, storing a u8 inside a bool
	*reinterpret_cast<u8*>(&val) = 2;
}

static void SetUndetermined(int& val)
{
	val = -1;
}

void VideoConfig::SetAllUndetermined()
{
	// video hardware
	SetUndetermined(bVSync);
	SetUndetermined(iAdapter);

	// video settings
	SetUndetermined(bWidescreenHack);
	SetUndetermined(iAspectRatio);
	SetUndetermined(bCrop);
	SetUndetermined(bUseXFB);
	SetUndetermined(bUseRealXFB);
	SetUndetermined(bUseNativeMips);

	SetUndetermined(bSafeTextureCache);
	SetUndetermined(iSafeTextureCache_ColorSamples);

	SetUndetermined(bShowFPS);
	SetUndetermined(bShowInputDisplay);
	SetUndetermined(bOverlayStats);
	SetUndetermined(bOverlayProjStats);
	SetUndetermined(bShowEFBCopyRegions);
	SetUndetermined(iCompileDLsLevel);
	SetUndetermined(bDumpTextures);
	SetUndetermined(bHiresTextures);
	SetUndetermined(bDumpEFBTarget);
	SetUndetermined(bDumpFrames);
	SetUndetermined(bFreeLook);
	SetUndetermined(bUseFFV1);
	SetUndetermined(bAnaglyphStereo);
	SetUndetermined(iAnaglyphStereoSeparation);
	SetUndetermined(iAnaglyphFocalAngle);
	SetUndetermined(bEnablePixelLighting);
	SetUndetermined(bEnablePerPixelDepth);

	SetUndetermined(bShowShaderErrors);
	SetUndetermined(iMultisampleMode);
	SetUndetermined(iEFBScale);

	SetUndetermined(bDstAlphaPass);

	SetUndetermined(bTexFmtOverlayEnable);
	SetUndetermined(bTexFmtOverlayCenter);
	SetUndetermined(bWireFrame);
	SetUndetermined(bDisableLighting);
	SetUndetermined(bDisableTexturing);
	SetUndetermined(bDisableFog);

	SetUndetermined(bEnableOpenCL);
#ifdef _OPENMP
	SetUndetermined(bOMPDecoder);
#endif

	// video enhancements
	SetUndetermined(bForceFiltering);
	SetUndetermined(iMaxAnisotropy);
	//SetUndetermined(sPostProcessingShader);
	SetUndetermined(b3DVision);

	// video hacks
	SetUndetermined(bEFBAccessEnable);
	SetUndetermined(bDlistCachingEnable);
	SetUndetermined(bEFBCopyEnable);
	SetUndetermined(bOSDHotKey);
	SetUndetermined(bCopyEFBToTexture);
	SetUndetermined(bCopyEFBScaled);
	SetUndetermined(bEFBCopyCacheEnable);
	SetUndetermined(bEFBEmulateFormatChanges);

	// this doesn't belong here, o well
	backend_info = BackendInfo();
}

void VideoConfig::GameIniSave(const char* game_ini)
{
	IniFile ini_file;
	ini_file.Load(game_ini);

	// video hardware
	IniFile::Section& vid_hardware = *ini_file.GetOrCreateSection("Video_Hardware");
	SetIfDetermined(vid_hardware, "VSync", bVSync);
	SetIfDetermined(vid_hardware, "Adapter", iAdapter);

	// video settings
	IniFile::Section& vid_settings = *ini_file.GetOrCreateSection("Video_Settings");
	SetIfDetermined(vid_settings, "wideScreenHack", bWidescreenHack);
	SetIfDetermined(vid_settings, "AspectRatio", iAspectRatio);
	SetIfDetermined(vid_settings, "Crop", bCrop);
	SetIfDetermined(vid_settings, "UseXFB", bUseXFB);
	SetIfDetermined(vid_settings, "UseRealXFB", bUseRealXFB);
	SetIfDetermined(vid_settings, "UseNativeMips", bUseNativeMips);

	SetIfDetermined(vid_settings, "SafeTextureCache", bSafeTextureCache);
	SetIfDetermined(vid_settings, "SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);

	SetIfDetermined(vid_settings, "ShowFPS", bShowFPS);
	SetIfDetermined(vid_settings, "ShowInputDisplay", bShowInputDisplay);
	SetIfDetermined(vid_settings, "OverlayStats", bOverlayStats);
	SetIfDetermined(vid_settings, "OverlayProjStats", bOverlayProjStats);
	SetIfDetermined(vid_settings, "ShowEFBCopyRegions", bShowEFBCopyRegions);
	SetIfDetermined(vid_settings, "DLOptimize", iCompileDLsLevel);
	SetIfDetermined(vid_settings, "DumpTextures", bDumpTextures);
	SetIfDetermined(vid_settings, "HiresTextures", bHiresTextures);
	SetIfDetermined(vid_settings, "DumpEFBTarget", bDumpEFBTarget);
	SetIfDetermined(vid_settings, "DumpFrames", bDumpFrames);
	SetIfDetermined(vid_settings, "FreeLook", bFreeLook);
	SetIfDetermined(vid_settings, "UseFFV1", bUseFFV1);
	SetIfDetermined(vid_settings, "AnaglyphStereo", bAnaglyphStereo);
	SetIfDetermined(vid_settings, "AnaglyphStereoSeparation", iAnaglyphStereoSeparation);
	SetIfDetermined(vid_settings, "AnaglyphFocalAngle", iAnaglyphFocalAngle);
	SetIfDetermined(vid_settings, "EnablePixelLighting", bEnablePixelLighting);
	SetIfDetermined(vid_settings, "EnablePerPixelDepth", bEnablePerPixelDepth);

	SetIfDetermined(vid_settings, "ShowShaderErrors", bShowShaderErrors);
	SetIfDetermined(vid_settings, "MSAA", iMultisampleMode);
	SetIfDetermined(vid_settings, "EFBScale", iEFBScale); // integral

	SetIfDetermined(vid_settings, "DstAlphaPass", bDstAlphaPass);

	SetIfDetermined(vid_settings, "TexFmtOverlayEnable", bTexFmtOverlayEnable);
	SetIfDetermined(vid_settings, "TexFmtOverlayCenter", bTexFmtOverlayCenter);
	SetIfDetermined(vid_settings, "WireFrame", bWireFrame);
	SetIfDetermined(vid_settings, "DisableLighting", bDisableLighting);
	SetIfDetermined(vid_settings, "DisableTexturing", bDisableTexturing);
	SetIfDetermined(vid_settings, "DisableFog", bDisableFog);

	SetIfDetermined(vid_settings, "EnableOpenCL", bEnableOpenCL);
#ifdef _OPENMP
	SetIfDetermined(vid_settings, "OMPDecoder", bOMPDecoder);
#endif

	// video enhancements
	IniFile::Section& vid_enhancements = *ini_file.GetOrCreateSection("Video_Enhancements");
	SetIfDetermined(vid_enhancements, "ForceFiltering", bForceFiltering);
	SetIfDetermined(vid_enhancements, "MaxAnisotropy", iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	//SetIfDetermined(vid_enhancements, "PostProcessingShader", sPostProcessingShader);
	SetIfDetermined(vid_enhancements, "Enable3dVision", b3DVision);

	// video hacks
	IniFile::Section& vid_hacks = *ini_file.GetOrCreateSection("Video_Hacks");
	SetIfDetermined(vid_hacks, "EFBAccessEnable", bEFBAccessEnable);
	SetIfDetermined(vid_hacks, "DlistCachingEnable", bDlistCachingEnable);
	SetIfDetermined(vid_hacks, "EFBCopyEnable", bEFBCopyEnable);
	SetIfDetermined(vid_hacks, "EFBCopyDisableHotKey", bOSDHotKey);
	SetIfDetermined(vid_hacks, "EFBToTextureEnable", bCopyEFBToTexture);
	SetIfDetermined(vid_hacks, "EFBScaledCopy", bCopyEFBScaled);
	SetIfDetermined(vid_hacks, "EFBCopyCacheEnable", bEFBCopyCacheEnable);
	SetIfDetermined(vid_hacks, "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

	ini_file.Save(game_ini);
}


// TODO: remove
extern bool g_aspect_wide;

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
	bool use16_9 = g_aspect_wide;
	
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
