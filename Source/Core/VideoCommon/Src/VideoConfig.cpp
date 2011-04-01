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

void VideoConfig::Load(const char *main_ini_file, bool filecheck_passed, const char *game_ini_file)
{
	std::string temp;
	IniFile iniFile;
	iniFile.Load(main_ini_file);

	#define SET_STATE(evaluate, member) evaluate; UI_State.member = true;

	SET_STATE(iniFile.Get("Hardware", "VSync", &bVSync, false), bVSync); // Hardware
	SET_STATE(iniFile.Get("Settings", "wideScreenHack", &bWidescreenHack, false), bWidescreenHack);
	SET_STATE(iniFile.Get("Settings", "AspectRatio", &iAspectRatio, (int)ASPECT_AUTO), iAspectRatio);
	SET_STATE(iniFile.Get("Settings", "Crop", &bCrop, false), bCrop);
	SET_STATE(iniFile.Get("Settings", "UseXFB", &bUseXFB, false), bUseXFB);
	iniFile.Get("Settings", "UseRealXFB", &bUseRealXFB, false);
	SET_STATE(iniFile.Get("Settings", "UseNativeMips", &bUseNativeMips, true), bUseNativeMips);
	
	SET_STATE(iniFile.Get("Settings", "SafeTextureCache", &bSafeTextureCache, false), bSafeTextureCache); // Settings
	//Safe texture cache params
	iniFile.Get("Settings", "SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples, 512);

	SET_STATE(iniFile.Get("Settings", "ShowFPS", &bShowFPS, false), bShowFPS); // Settings
	SET_STATE(iniFile.Get("Settings", "ShowInputDisplay", &bShowInputDisplay, false), bShowInputDisplay);
	SET_STATE(iniFile.Get("Settings", "OverlayStats", &bOverlayStats, false), bOverlayStats);
	SET_STATE(iniFile.Get("Settings", "OverlayProjStats", &bOverlayProjStats, false), bOverlayProjStats);
	SET_STATE(iniFile.Get("Settings", "ShowEFBCopyRegions", &bShowEFBCopyRegions, false), bShowEFBCopyRegions);
	SET_STATE(iniFile.Get("Settings", "DLOptimize", &iCompileDLsLevel, 0), iCompileDLsLevel);
	SET_STATE(iniFile.Get("Settings", "DumpTextures", &bDumpTextures, false), bDumpTextures);
	SET_STATE(iniFile.Get("Settings", "HiresTextures", &bHiresTextures, false), bHiresTextures);
	SET_STATE(iniFile.Get("Settings", "DumpEFBTarget", &bDumpEFBTarget, false), bDumpEFBTarget) ;
	SET_STATE(iniFile.Get("Settings", "DumpFrames", &bDumpFrames, false), bDumpFrames);
	SET_STATE(iniFile.Get("Settings", "FreeLook", &bFreeLook, false), bFreeLook);
	SET_STATE(iniFile.Get("Settings", "UseFFV1", &bUseFFV1, false), bUseFFV1);
	SET_STATE(iniFile.Get("Settings", "AnaglyphStereo", &bAnaglyphStereo, false), bAnaglyphStereo);
	SET_STATE(iniFile.Get("Settings", "AnaglyphStereoSeparation", &iAnaglyphStereoSeparation, 200), iAnaglyphStereoSeparation);
	SET_STATE(iniFile.Get("Settings", "AnaglyphFocalAngle", &iAnaglyphFocalAngle, 0), iAnaglyphFocalAngle);
	SET_STATE(iniFile.Get("Settings", "EnablePixelLighting", &bEnablePixelLighting, false), bEnablePixelLighting);
	SET_STATE(iniFile.Get("Settings", "EnablePerPixelDepth", &bEnablePerPixelDepth, false), bEnablePerPixelDepth);
	
	SET_STATE(iniFile.Get("Settings", "ShowShaderErrors", &bShowShaderErrors, false), bShowShaderErrors);
	SET_STATE(iniFile.Get("Settings", "MSAA", &iMultisampleMode, 0), iMultisampleMode);
	SET_STATE(iniFile.Get("Settings", "EFBScale", &iEFBScale, 1), iEFBScale); // integral
	
	SET_STATE(iniFile.Get("Settings", "DstAlphaPass", &bDstAlphaPass, false), bDstAlphaPass);
	
	SET_STATE(iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, false), bTexFmtOverlayEnable);
	SET_STATE(iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, false), bTexFmtOverlayCenter);
	SET_STATE(iniFile.Get("Settings", "WireFrame", &bWireFrame, false), bWireFrame);
	SET_STATE(iniFile.Get("Settings", "DisableLighting", &bDisableLighting, false), bDisableLighting);
	SET_STATE(iniFile.Get("Settings", "DisableTexturing", &bDisableTexturing, false), bDisableTexturing);
	SET_STATE(iniFile.Get("Settings", "DisableFog", &bDisableFog, false), bDisableFog);
	
	SET_STATE(iniFile.Get("Settings", "EnableOpenCL", &bEnableOpenCL, false), bEnableOpenCL);
#ifdef _OPENMP
	SET_STATE(iniFile.Get("Settings", "OMPDecoder", &bOMPDecoder, false), bOMPDecoder);
#endif

	SET_STATE(iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, false), bForceFiltering);
	SET_STATE(iniFile.Get("Enhancements", "ForceNoFiltering", &bForceNoFiltering, false), bForceNoFiltering);
	SET_STATE(iniFile.Get("Enhancements", "MaxAnisotropy", &iMaxAnisotropy, 0), iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	iniFile.Get("Enhancements", "PostProcessingShader", &sPostProcessingShader, "");
	SET_STATE(iniFile.Get("Enhancements", "Enable3dVision", &b3DVision, false), b3DVision);
	
	SET_STATE(iniFile.Get("Hacks", "EFBAccessEnable", &bEFBAccessEnable, true), bEFBAccessEnable);
	SET_STATE(iniFile.Get("Hacks", "DlistCachingEnable", &bDlistCachingEnable,false), bDlistCachingEnable);
	SET_STATE(iniFile.Get("Hacks", "EFBCopyEnable", &bEFBCopyEnable, true), bEFBCopyEnable);
	SET_STATE(iniFile.Get("Hacks", "EFBCopyDisableHotKey", &bOSDHotKey, false), bOSDHotKey);
	iniFile.Get("Hacks", "EFBToTextureEnable", &bCopyEFBToTexture, false);
	SET_STATE(iniFile.Get("Hacks", "EFBScaledCopy", &bCopyEFBScaled, true), bCopyEFBScaled);
	SET_STATE(iniFile.Get("Hacks", "EFBCopyCacheEnable", &bEFBCopyCacheEnable, false), bEFBCopyCacheEnable);
	SET_STATE(iniFile.Get("Hacks", "EFBEmulateFormatChanges", &bEFBEmulateFormatChanges, true), bEFBEmulateFormatChanges);

	iniFile.Get("Hardware", "Adapter", &iAdapter, 0);
	if (iAdapter == -1)
		iAdapter = 0;
	SET_STATE((void)true, iAdapter);
	
	// Load common settings
	iniFile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	bool bTmp;
	iniFile.Get("Interface", "UsePanicHandlers", &bTmp, true);
	SetEnableAlert(bTmp);
	if (filecheck_passed)
		GameIniLoad(game_ini_file);
}

void VideoConfig::GameIniLoad(const char *ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);

	#define SET_UISTATE(check, member) UI_State.member = (check) ? true : false

	SET_UISTATE(iniFile.GetIfExists("Video_Hardware", "VSync", &bVSync), bVSync);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "wideScreenHack", &bWidescreenHack), bWidescreenHack);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "AspectRatio", &iAspectRatio), iAspectRatio);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "Crop", &bCrop), bCrop);
	
	{ // CheckBox+RadioButtons group
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "UseXFB", &bUseXFB), bUseXFB);
	if (UI_State.bUseXFB)
		iniFile.GetIfExists("Video_Settings", "UseRealXFB", &bUseRealXFB);
	}
	
	{ // CheckBox+RadioButtons group
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "SafeTextureCache", &bSafeTextureCache), bSafeTextureCache);
	if (UI_State.bSafeTextureCache)
		iniFile.GetIfExists("Video_Settings", "SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples);
	}
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "UseNativeMips", &bUseNativeMips), bUseNativeMips);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "ShowFPS", &bShowFPS), bShowFPS);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "ShowInputDisplay", &bShowInputDisplay), bShowInputDisplay);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "OverlayStats", &bOverlayStats), bOverlayStats);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "OverlayProjStats", &bOverlayProjStats), bOverlayProjStats);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "ShowEFBCopyRegions", &bShowEFBCopyRegions), bShowEFBCopyRegions);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "DLOptimize", &iCompileDLsLevel), iCompileDLsLevel);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "DumpTextures", &bDumpTextures), bDumpTextures);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "HiresTextures", &bHiresTextures), bHiresTextures);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "DumpEFBTarget", &bDumpEFBTarget), bDumpEFBTarget);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "DumpFrames", &bDumpFrames), bDumpFrames);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "FreeLook", &bFreeLook), bFreeLook);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "UseFFV1", &bUseFFV1), bUseFFV1);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "AnaglyphStereo", &bAnaglyphStereo), bAnaglyphStereo);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "AnaglyphStereoSeparation", &iAnaglyphStereoSeparation), iAnaglyphStereoSeparation);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "AnaglyphFocalAngle", &iAnaglyphFocalAngle), iAnaglyphFocalAngle);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "EnablePixelLighting", &bEnablePixelLighting), bEnablePixelLighting);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "EnablePerPixelDepth", &bEnablePerPixelDepth), bEnablePerPixelDepth);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "ShowShaderErrors", &bShowShaderErrors), bShowShaderErrors);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "MSAA", &iMultisampleMode), iMultisampleMode);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "EFBScale", &iEFBScale), iEFBScale); // integral
	
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "DstAlphaPass", &bDstAlphaPass), bDstAlphaPass);

	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable), bTexFmtOverlayEnable);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter), bTexFmtOverlayCenter);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "WireFrame", &bWireFrame), bWireFrame);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "DisableLighting", &bDisableLighting), bDisableLighting);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "DisableTexturing", &bDisableTexturing), bDisableTexturing);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "DisableFog", &bDisableFog), bDisableFog);
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "EnableOpenCL", &bEnableOpenCL), bEnableOpenCL);
#ifdef _OPENMP
	SET_UISTATE(iniFile.GetIfExists("Video_Settings", "OMPDecoder", &bOMPDecoder), bOMPDecoder);
#endif
	SET_UISTATE(iniFile.GetIfExists("Video_Enhancements", "ForceFiltering", &bForceFiltering), bForceFiltering);
	SET_UISTATE(iniFile.GetIfExists("Video_Enhancements", "ForceNoFiltering", &bForceNoFiltering), bForceNoFiltering);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Enhancements", "MaxAnisotropy", &iMaxAnisotropy), iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	iniFile.GetIfExists("Video_Enhancements", "PostProcessingShader", &sPostProcessingShader);
	
	SET_UISTATE(iniFile.GetIfExists("Video_Enhancements", "Enable3dVision", &b3DVision), b3DVision);

	SET_UISTATE(iniFile.GetIfExists("Video_Hacks", "EFBAccessEnable", &bEFBAccessEnable), bEFBAccessEnable);
	SET_UISTATE(iniFile.GetIfExists("Video_Hacks", "DlistCachingEnable", &bDlistCachingEnable), bDlistCachingEnable);
	SET_UISTATE(iniFile.GetIfExists("Video_Hacks", "EFBCopyDisableHotKey", &bOSDHotKey), bOSDHotKey);
	
	{ // CheckBox+RadioButtons group
	SET_UISTATE(iniFile.GetIfExists("Video_Hacks", "EFBCopyEnable", &bEFBCopyEnable), bEFBCopyEnable);
	if (UI_State.bEFBCopyEnable)
		iniFile.GetIfExists("Video_Hacks", "EFBToTextureEnable", &bCopyEFBToTexture);

	SET_UISTATE(iniFile.GetIfExists("Video_Hacks", "EFBCopyCacheEnable", &bEFBCopyCacheEnable), bEFBCopyCacheEnable);
	}
	
	SET_UISTATE(iniFile.GetIfExists("Video_Hacks", "EFBScaledCopy", &bCopyEFBScaled), bCopyEFBScaled);
	SET_UISTATE(iniFile.GetIfExists("Video_Hacks", "EFBEmulateFormatChanges", &bEFBEmulateFormatChanges), bEFBEmulateFormatChanges);

	SET_UISTATE(iniFile.GetIfExists("Video_Hardware", "Adapter", &iAdapter), iAdapter);
	
	iniFile.GetIfExists("Video", "ProjectionHack", &iPhackvalue[0]);
	iniFile.GetIfExists("Video", "PH_SZNear", &iPhackvalue[1]);
	iniFile.GetIfExists("Video", "PH_SZFar", &iPhackvalue[2]);
	iniFile.GetIfExists("Video", "PH_ExtraParam", &iPhackvalue[3]);
	iniFile.GetIfExists("Video", "PH_ZNear", &sPhackvalue[0]);
	iniFile.GetIfExists("Video", "PH_ZFar", &sPhackvalue[1]);

	SET_UISTATE(iniFile.GetIfExists("Video", "ZTPSpeedupHack", &bZTPSpeedHack), bZTPSpeedHack);

	VerifyValidity();
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
	iniFile.Set("Settings", "WireFrame", bWireFrame);
	iniFile.Set("Settings", "DisableLighting", bDisableLighting);
	iniFile.Set("Settings", "DisableTexturing", bDisableTexturing);
	iniFile.Set("Settings", "DstAlphaPass", bDstAlphaPass);
	iniFile.Set("Settings", "DisableFog", bDisableFog);

	iniFile.Set("Settings", "EnableOpenCL", bEnableOpenCL);
#ifdef _OPENMP
	iniFile.Set("Settings", "OMPDecoder", bOMPDecoder);
#endif
	
	iniFile.Set("Enhancements", "ForceFiltering", bForceFiltering);
	iniFile.Set("Enhancements", "ForceNoFiltering", bForceNoFiltering);
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

	IniFile iniFile;
	iniFile.Load(game_ini);

	#define CHECK_UISTATE(section, key, member){\
		if (UI_State.member) iniFile.Set((section), (key), (member)); else iniFile.DeleteKey((section), (key)); }

	CHECK_UISTATE("Video_Hardware", "VSync", bVSync);
	CHECK_UISTATE("Video_Settings", "wideScreenHack", bWidescreenHack);
	CHECK_UISTATE("Video_Settings", "AspectRatio", iAspectRatio);
	CHECK_UISTATE("Video_Settings", "Crop", bCrop);

	{ // CheckBox+RadioButtons group
	CHECK_UISTATE("Video_Settings", "UseXFB", bUseXFB);
	UI_State.bUseRealXFB = UI_State.bUseXFB;
	CHECK_UISTATE("Video_Settings", "UseRealXFB", bUseRealXFB);
	}

	CHECK_UISTATE("Video_Settings", "UseNativeMips", bUseNativeMips);
	
	{ // CheckBox+RadioButtons group
	CHECK_UISTATE("Video_Settings", "SafeTextureCache", bSafeTextureCache);
	UI_State.iSafeTextureCache_ColorSamples = UI_State.bSafeTextureCache;
	CHECK_UISTATE("Video_Settings", "SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);
	}

	CHECK_UISTATE("Video_Settings", "ShowFPS", bShowFPS);
	CHECK_UISTATE("Video_Settings", "ShowInputDisplay", bShowInputDisplay);
	CHECK_UISTATE("Video_Settings", "OverlayStats", bOverlayStats);
	CHECK_UISTATE("Video_Settings", "OverlayProjStats", bOverlayProjStats);
	CHECK_UISTATE("Video_Settings", "ShowEFBCopyRegions", bShowEFBCopyRegions);
	CHECK_UISTATE("Video_Settings", "DLOptimize", iCompileDLsLevel);
	CHECK_UISTATE("Video_Settings", "DumpTextures", bDumpTextures);
	CHECK_UISTATE("Video_Settings", "HiresTextures", bHiresTextures);
	CHECK_UISTATE("Video_Settings", "DumpEFBTarget", bDumpEFBTarget);
	CHECK_UISTATE("Video_Settings", "DumpFrames", bDumpFrames);
	CHECK_UISTATE("Video_Settings", "FreeLook", bFreeLook);
	CHECK_UISTATE("Video_Settings", "UseFFV1", bUseFFV1);
	CHECK_UISTATE("Video_Settings", "AnaglyphStereo", bAnaglyphStereo);
	CHECK_UISTATE("Video_Settings", "AnaglyphStereoSeparation", iAnaglyphStereoSeparation);
	CHECK_UISTATE("Video_Settings", "AnaglyphFocalAngle", iAnaglyphFocalAngle);
	CHECK_UISTATE("Video_Settings", "EnablePixelLighting", bEnablePixelLighting);
	CHECK_UISTATE("Video_Settings", "EnablePerPixelDepth", bEnablePerPixelDepth);

	CHECK_UISTATE("Video_Settings", "ShowShaderErrors", bShowShaderErrors);
	CHECK_UISTATE("Video_Settings", "MSAA", iMultisampleMode);
	CHECK_UISTATE("Video_Settings", "EFBScale", iEFBScale); // integral

	CHECK_UISTATE("Video_Settings", "DstAlphaPass", bDstAlphaPass);

	CHECK_UISTATE("Video_Settings", "TexFmtOverlayEnable", bTexFmtOverlayEnable);
	CHECK_UISTATE("Video_Settings", "TexFmtOverlayCenter", bTexFmtOverlayCenter);
	CHECK_UISTATE("Video_Settings", "WireFrame", bWireFrame);
	CHECK_UISTATE("Video_Settings", "DisableLighting", bDisableLighting);
	CHECK_UISTATE("Video_Settings", "DisableTexturing", bDisableTexturing);
	CHECK_UISTATE("Video_Settings", "DisableFog", bDisableFog);

	CHECK_UISTATE("Video_Settings", "EnableOpenCL", bEnableOpenCL);
#ifdef _OPENMP
	CHECK_UISTATE("Video_Settings", "OMPDecoder", bOMPDecoder);
#endif

	CHECK_UISTATE("Video_Enhancements", "ForceFiltering", bForceFiltering);
	CHECK_UISTATE("Video_Enhancements", "ForceNoFiltering", bForceNoFiltering);
	CHECK_UISTATE("Video_Enhancements", "MaxAnisotropy", iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	iniFile.Set("Video_Enhancements", "PostProcessingShader", sPostProcessingShader);
	CHECK_UISTATE("Video_Enhancements", "Enable3dVision", b3DVision);

	CHECK_UISTATE("Video_Hacks", "EFBAccessEnable", bEFBAccessEnable);
	CHECK_UISTATE("Video_Hacks", "DlistCachingEnable", bDlistCachingEnable);
	CHECK_UISTATE("Video_Hacks", "EFBCopyDisableHotKey", bOSDHotKey);
	
	{ // CheckBox+RadioButtons group
	CHECK_UISTATE("Video_Hacks", "EFBCopyEnable", bEFBCopyEnable);
	UI_State.bCopyEFBToTexture = UI_State.bEFBCopyEnable;
	CHECK_UISTATE("Video_Hacks", "EFBToTextureEnable", bCopyEFBToTexture);

	CHECK_UISTATE("Video_Hacks", "EFBCopyCacheEnable", bEFBCopyCacheEnable);
	}

	CHECK_UISTATE("Video_Hacks", "EFBScaledCopy", bCopyEFBScaled);
	CHECK_UISTATE("Video_Hacks", "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

	CHECK_UISTATE("Video_Hardware", "Adapter", iAdapter);

	iniFile.Save(game_ini);
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
