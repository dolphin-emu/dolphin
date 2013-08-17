// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "Common.h"
#include "IniFile.h"
#include "VideoConfig.h"
#include "VideoCommon.h"
#include "FileUtil.h"
#include "Core.h"
#include "Movie.h"
#include "OnScreenDisplay.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;

void UpdateActiveConfig()
{
	if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
		Movie::SetGraphicsConfig();
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
	backend_info.bUseMinimalMipCount = false;
	backend_info.bSupports3DVision = false;
}

void VideoConfig::Load(const char *ini_file)
{
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
	iniFile.Get("Settings", "HackedBufferUpload", &bHackedBufferUpload, 0);
	iniFile.Get("Settings", "FastDepthCalc", &bFastDepthCalc, true);

	iniFile.Get("Settings", "MSAA", &iMultisampleMode, 0);
	iniFile.Get("Settings", "EFBScale", &iEFBScale, (int) SCALE_1X); // native

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

	// Shader Debugging causes a huge slowdown and it's easy to forget about it
	// since it's not exposed in the settings dialog. It's only used by
	// developers, so displaying an obnoxious message avoids some confusion and
	// is not too annoying/confusing for users.
	//
	// XXX(delroth): This is kind of a bad place to put this, but the current
	// VideoCommon is a mess and we don't have a central initialization
	// function to do these kind of checks. Instead, the init code is
	// triplicated for each video backend.
	if (bEnableShaderDebugging)
		OSD::AddMessage("Warning: Shader Debugging is enabled, performance will suffer heavily", 15000);
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
	iniFile.GetIfExists("Video_Settings", "HackedBufferUpload", &bHackedBufferUpload);
	iniFile.GetIfExists("Video_Settings", "FastDepthCalc", &bFastDepthCalc);
	iniFile.GetIfExists("Video_Settings", "MSAA", &iMultisampleMode);
	int tmp = -9000;
	iniFile.GetIfExists("Video_Settings", "EFBScale", &tmp); // integral
	if (tmp != -9000)
	{
		if (tmp != SCALE_FORCE_INTEGRAL)
		{
			iEFBScale = tmp;
		}
		else // Round down to multiple of native IR
		{
			switch (iEFBScale)
			{
			case SCALE_AUTO:
				iEFBScale = SCALE_AUTO_INTEGRAL;
				break;
			case SCALE_1_5X:
				iEFBScale = SCALE_1X;
				break;
			case SCALE_2_5X:
				iEFBScale = SCALE_2X;
				break;
			default:
				break;
			}
		}
	}

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
	iniFile.GetIfExists("Video", "PerfQueriesEnable", &bPerfQueriesEnable);
}

void VideoConfig::VerifyValidity()
{
	// TODO: Check iMaxAnisotropy value
	if (iAdapter < 0 || iAdapter > ((int)backend_info.Adapters.size() - 1)) iAdapter = 0;
	if (iMultisampleMode < 0 || iMultisampleMode >= (int)backend_info.AAModes.size()) iMultisampleMode = 0;
	if (!backend_info.bSupports3DVision) b3DVision = false;
	if (!backend_info.bSupportsFormatReinterpretation) bEFBEmulateFormatChanges = false;
	if (!backend_info.bSupportsPixelLighting) bEnablePixelLighting = false;
	if (backend_info.APIType != API_OPENGL) backend_info.bSupportsGLSLUBO = false;
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
	iniFile.Set("Settings", "HackedBufferUpload", bHackedBufferUpload);
	iniFile.Set("Settings", "FastDepthCalc", bFastDepthCalc);

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
	SET_IF_DIFFERS("Video_Settings", "FastDepthCalc", bFastDepthCalc);
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

bool VideoConfig::IsVSync()
{
	return Core::isTabPressed ? false : bVSync;
}
