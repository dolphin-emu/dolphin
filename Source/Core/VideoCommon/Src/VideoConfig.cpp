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
	iniFile.Get("Settings", "SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples,128);
	iniFile.Get("Settings", "ShowFPS", &bShowFPS, false); // Settings
	iniFile.Get("Settings", "LogFPSToFile", &bLogFPSToFile, false);
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
	
	iniFile.Get("Settings", "MSAA", &iMultisampleMode, 0);
	iniFile.Get("Settings", "EFBScale", &iEFBScale, 2); // native
	
	iniFile.Get("Settings", "DstAlphaPass", &bDstAlphaPass, false);
	
	iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
	iniFile.Get("Settings", "WireFrame", &bWireFrame, 0);
	iniFile.Get("Settings", "DisableFog", &bDisableFog, 0);
	
	iniFile.Get("Settings", "EnableOpenCL", &bEnableOpenCL, false);
	iniFile.Get("Settings", "OMPDecoder", &bOMPDecoder, false);

	iniFile.Get("Settings", "EnableShaderDebugging", &bEnableShaderDebugging, false);

	iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
	iniFile.Get("Enhancements", "MaxAnisotropy", &iMaxAnisotropy, 0);  // NOTE - this is x in (1 << x)
	iniFile.Get("Enhancements", "PostProcessingShader", &sPostProcessingShader, "");
	iniFile.Get("Enhancements", "Enable3dVision", &b3DVision, false);
	
	iniFile.Get("Hacks", "EFBAccessEnable", &bEFBAccessEnable, true);
	iniFile.Get("Hacks", "DlistCachingEnable", &bDlistCachingEnable,false);
	iniFile.Get("Hacks", "EFBCopyEnable", &bEFBCopyEnable, true);
	iniFile.Get("Hacks", "EFBCopyDisableHotKey", &bOSDHotKey, 0);
	iniFile.Get("Hacks", "EFBToTextureEnable", &bCopyEFBToTexture, true);
	iniFile.Get("Hacks", "EFBScaledCopy", &bCopyEFBScaled, true);
	iniFile.Get("Hacks", "EFBCopyCacheEnable", &bEFBCopyCacheEnable, false);
	iniFile.Get("Hacks", "EFBEmulateFormatChanges", &bEFBEmulateFormatChanges, false);

	iniFile.Get("Hardware", "Adapter", &iAdapter, 0);

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
	iniFile.GetIfExists("Video_Settings", "SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples);
	iniFile.GetIfExists("Video_Settings", "DLOptimize", &iCompileDLsLevel);
	iniFile.GetIfExists("Video_Settings", "HiresTextures", &bHiresTextures);
	iniFile.GetIfExists("Video_Settings", "AnaglyphStereo", &bAnaglyphStereo);
	iniFile.GetIfExists("Video_Settings", "AnaglyphStereoSeparation", &iAnaglyphStereoSeparation);
	iniFile.GetIfExists("Video_Settings", "AnaglyphFocalAngle", &iAnaglyphFocalAngle);
	iniFile.GetIfExists("Video_Settings", "EnablePixelLighting", &bEnablePixelLighting);
	iniFile.GetIfExists("Video_Settings", "EnablePerPixelDepth", &bEnablePerPixelDepth);
	iniFile.GetIfExists("Video_Settings", "MSAA", &iMultisampleMode);
	iniFile.GetIfExists("Video_Settings", "EFBScale", &iEFBScale); // integral
	iniFile.GetIfExists("Video_Settings", "DstAlphaPass", &bDstAlphaPass);
	iniFile.GetIfExists("Video_Settings", "DisableFog", &bDisableFog);
	iniFile.GetIfExists("Video_Settings", "EnableOpenCL", &bEnableOpenCL);
	iniFile.GetIfExists("Video_Settings", "OMPDecoder", &bOMPDecoder);

	iniFile.GetIfExists("Video_Enhancements", "ForceFiltering", &bForceFiltering);
	iniFile.GetIfExists("Video_Enhancements", "MaxAnisotropy", &iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	iniFile.GetIfExists("Video_Enhancements", "PostProcessingShader", &sPostProcessingShader);
	iniFile.GetIfExists("Video_Enhancements", "Enable3dVision", &b3DVision);

	iniFile.GetIfExists("Video_Hacks", "EFBAccessEnable", &bEFBAccessEnable);
	iniFile.GetIfExists("Video_Hacks", "DlistCachingEnable", &bDlistCachingEnable);
	iniFile.GetIfExists("Video_Hacks", "EFBCopyEnable", &bEFBCopyEnable);
	iniFile.GetIfExists("Video_Hacks", "EFBToTextureEnable", &bCopyEFBToTexture);
	iniFile.GetIfExists("Video_Hacks", "EFBScaledCopy", &bCopyEFBScaled);
	iniFile.GetIfExists("Video_Hacks", "EFBCopyCacheEnable", &bEFBCopyCacheEnable);
	iniFile.GetIfExists("Video_Hacks", "EFBEmulateFormatChanges", &bEFBEmulateFormatChanges);

	iniFile.GetIfExists("Video", "ProjectionHack", &iPhackvalue[0]);
	iniFile.GetIfExists("Video", "PH_SZNear", &iPhackvalue[1]);
	iniFile.GetIfExists("Video", "PH_SZFar", &iPhackvalue[2]);
	iniFile.GetIfExists("Video", "PH_ExtraParam", &iPhackvalue[3]);
	iniFile.GetIfExists("Video", "PH_ZNear", &sPhackvalue[0]);
	iniFile.GetIfExists("Video", "PH_ZFar", &sPhackvalue[1]);
	iniFile.GetIfExists("Video", "ZTPSpeedupHack", &bZTPSpeedHack);
	iniFile.GetIfExists("Video", "UseBBox", &bUseBBox);
}

void VideoConfig::VerifyValidity()
{
	// TODO: Check iMaxAnisotropy value
	if (iAdapter < 0 || iAdapter > ((int)backend_info.Adapters.size() - 1)) iAdapter = 0;
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
	iniFile.Set("Settings", "SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);
	iniFile.Set("Settings", "ShowFPS", bShowFPS);
	iniFile.Set("Settings", "LogFPSToFile", bLogFPSToFile);
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
	iniFile.Set("Settings", "MSAA", iMultisampleMode);
	iniFile.Set("Settings", "EFBScale", iEFBScale);
	iniFile.Set("Settings", "TexFmtOverlayEnable", bTexFmtOverlayEnable);
	iniFile.Set("Settings", "TexFmtOverlayCenter", bTexFmtOverlayCenter);
	iniFile.Set("Settings", "Wireframe", bWireFrame);
	iniFile.Set("Settings", "DstAlphaPass", bDstAlphaPass);
	iniFile.Set("Settings", "DisableFog", bDisableFog);

	iniFile.Set("Settings", "EnableOpenCL", bEnableOpenCL);
	iniFile.Set("Settings", "OMPDecoder", bOMPDecoder);
	
	iniFile.Set("Settings", "EnableShaderDebugging", bEnableShaderDebugging);

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

void VideoConfig::GameIniSave(const char* default_ini, const char* game_ini)
{
	// wxWidgets doesn't provide us with a nice way to change 3-state checkboxes into 2-state ones
	// This would allow us to make the "default config" dialog layout to be 2-state based, but the
	// "game config" layout to be 3-state based (with the 3rd state being "use default")
	// Since we can't do that, we instead just save anything which differs from the default config
	// TODO: Make this less ugly

	VideoConfig defCfg;
	defCfg.Load(default_ini);

	IniFile iniFile;
	iniFile.Load(game_ini);

	#define SET_IF_DIFFERS(section, key, member) { if ((member) != (defCfg.member)) iniFile.Set((section), (key), (member)); else iniFile.DeleteKey((section), (key)); }

	SET_IF_DIFFERS("Video_Hardware", "VSync", bVSync);

	SET_IF_DIFFERS("Video_Settings", "wideScreenHack", bWidescreenHack);
	SET_IF_DIFFERS("Video_Settings", "AspectRatio", iAspectRatio);
	SET_IF_DIFFERS("Video_Settings", "Crop", bCrop);
	SET_IF_DIFFERS("Video_Settings", "UseXFB", bUseXFB);
	SET_IF_DIFFERS("Video_Settings", "UseRealXFB", bUseRealXFB);
	SET_IF_DIFFERS("Video_Settings", "SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);
	SET_IF_DIFFERS("Video_Settings", "DLOptimize", iCompileDLsLevel);
	SET_IF_DIFFERS("Video_Settings", "HiresTextures", bHiresTextures);
	SET_IF_DIFFERS("Video_Settings", "AnaglyphStereo", bAnaglyphStereo);
	SET_IF_DIFFERS("Video_Settings", "AnaglyphStereoSeparation", iAnaglyphStereoSeparation);
	SET_IF_DIFFERS("Video_Settings", "AnaglyphFocalAngle", iAnaglyphFocalAngle);
	SET_IF_DIFFERS("Video_Settings", "EnablePixelLighting", bEnablePixelLighting);
	SET_IF_DIFFERS("Video_Settings", "EnablePerPixelDepth", bEnablePerPixelDepth);
	SET_IF_DIFFERS("Video_Settings", "MSAA", iMultisampleMode);
	SET_IF_DIFFERS("Video_Settings", "EFBScale", iEFBScale); // integral
	SET_IF_DIFFERS("Video_Settings", "DstAlphaPass", bDstAlphaPass);
	SET_IF_DIFFERS("Video_Settings", "DisableFog", bDisableFog);
	SET_IF_DIFFERS("Video_Settings", "EnableOpenCL", bEnableOpenCL);
	SET_IF_DIFFERS("Video_Settings", "OMPDecoder", bOMPDecoder);

	SET_IF_DIFFERS("Video_Enhancements", "ForceFiltering", bForceFiltering);
	SET_IF_DIFFERS("Video_Enhancements", "MaxAnisotropy", iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	SET_IF_DIFFERS("Video_Enhancements", "PostProcessingShader", sPostProcessingShader);
	SET_IF_DIFFERS("Video_Enhancements", "Enable3dVision", b3DVision);

	SET_IF_DIFFERS("Video_Hacks", "EFBAccessEnable", bEFBAccessEnable);
	SET_IF_DIFFERS("Video_Hacks", "DlistCachingEnable", bDlistCachingEnable);
	SET_IF_DIFFERS("Video_Hacks", "EFBCopyEnable", bEFBCopyEnable);
	SET_IF_DIFFERS("Video_Hacks", "EFBToTextureEnable", bCopyEFBToTexture);
	SET_IF_DIFFERS("Video_Hacks", "EFBScaledCopy", bCopyEFBScaled);
	SET_IF_DIFFERS("Video_Hacks", "EFBCopyCacheEnable", bEFBCopyCacheEnable);
	SET_IF_DIFFERS("Video_Hacks", "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

	iniFile.Save(game_ini);
}
