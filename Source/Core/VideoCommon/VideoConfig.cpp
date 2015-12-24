// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

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

	// Exclusive fullscreen flags
	bFullscreen = false;
	bExclusiveMode = false;

	// Needed for the first frame, I think
	fAspectRatioHackW = 1;
	fAspectRatioHackH = 1;

	// disable all features by default
	backend_info.APIType = API_NONE;
	backend_info.bSupportsExclusiveFullscreen = false;

	// Game-specific stereoscopy settings
	bStereoEFBMonoDepth = false;
	iStereoDepthPercentage = 100;
	iStereoConvergence = 20;
}

void VideoConfig::Load(const std::string& ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);

	IniFile::Section* hardware = iniFile.GetOrCreateSection("Hardware");
	hardware->Get("VSync", &bVSync, 0);
	hardware->Get("Adapter", &iAdapter, 0);

	IniFile::Section* settings = iniFile.GetOrCreateSection("Settings");
	settings->Get("wideScreenHack", &bWidescreenHack, false);
	settings->Get("AspectRatio", &iAspectRatio, (int)ASPECT_AUTO);
	settings->Get("Crop", &bCrop, false);
	settings->Get("UseXFB", &bUseXFB, 0);
	settings->Get("UseRealXFB", &bUseRealXFB, 0);
	settings->Get("SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples, 128);
	settings->Get("ShowFPS", &bShowFPS, false);
	settings->Get("LogRenderTimeToFile", &bLogRenderTimeToFile, false);
	settings->Get("OverlayStats", &bOverlayStats, false);
	settings->Get("OverlayProjStats", &bOverlayProjStats, false);
	settings->Get("DumpTextures", &bDumpTextures, 0);
	settings->Get("HiresTextures", &bHiresTextures, 0);
	settings->Get("ConvertHiresTextures", &bConvertHiresTextures, 0);
	settings->Get("CacheHiresTextures", &bCacheHiresTextures, 0);
	settings->Get("DumpEFBTarget", &bDumpEFBTarget, 0);
	settings->Get("FreeLook", &bFreeLook, 0);
	settings->Get("UseFFV1", &bUseFFV1, 0);
	settings->Get("EnablePixelLighting", &bEnablePixelLighting, 0);
	settings->Get("FastDepthCalc", &bFastDepthCalc, true);
	settings->Get("MSAA", &iMultisamples, 1);
	settings->Get("SSAA", &bSSAA, false);
	settings->Get("EFBScale", &iEFBScale, (int)SCALE_1X); // native
	settings->Get("TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	settings->Get("TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
	settings->Get("WireFrame", &bWireFrame, 0);
	settings->Get("DisableFog", &bDisableFog, 0);
	settings->Get("EnableShaderDebugging", &bEnableShaderDebugging, false);
	settings->Get("BorderlessFullscreen", &bBorderlessFullscreen, false);

	IniFile::Section* enhancements = iniFile.GetOrCreateSection("Enhancements");
	enhancements->Get("ForceFiltering", &bForceFiltering, 0);
	enhancements->Get("MaxAnisotropy", &iMaxAnisotropy, 0);  // NOTE - this is x in (1 << x)
	enhancements->Get("PostProcessingShader", &sPostProcessingShader, "");

	IniFile::Section* stereoscopy = iniFile.GetOrCreateSection("Stereoscopy");
	stereoscopy->Get("StereoMode", &iStereoMode, 0);
	stereoscopy->Get("StereoDepth", &iStereoDepth, 20);
	stereoscopy->Get("StereoConvergencePercentage", &iStereoConvergencePercentage, 100);
	stereoscopy->Get("StereoSwapEyes", &bStereoSwapEyes, false);

	//currently these settings are not saved in global config, so we could've initialized them directly
	for (size_t i = 0; i < oStereoPresets.size(); ++i)
	{
		stereoscopy->Get(StringFromFormat("StereoConvergencePercentage_%zu", i), &oStereoPresets[i].convergence, iStereoConvergencePercentage);
		stereoscopy->Get(StringFromFormat("StereoDepth_%zu", i), &oStereoPresets[i].depth, iStereoDepth);
	}
	stereoscopy->Get("StereoActivePreset", &iStereoActivePreset, 0);
	iStereoConvergencePercentage = oStereoPresets[iStereoActivePreset].convergence;
	iStereoDepth = oStereoPresets[iStereoActivePreset].depth;

	IniFile::Section* hacks = iniFile.GetOrCreateSection("Hacks");
	hacks->Get("EFBAccessEnable", &bEFBAccessEnable, true);
	hacks->Get("BBoxEnable", &bBBoxEnable, false);
	hacks->Get("ForceProgressive", &bForceProgressive, true);
	hacks->Get("EFBToTextureEnable", &bSkipEFBCopyToRam, true);
	hacks->Get("EFBScaledCopy", &bCopyEFBScaled, true);
	hacks->Get("EFBEmulateFormatChanges", &bEFBEmulateFormatChanges, false);

	// hacks which are disabled by default
	iPhackvalue[0] = 0;
	bPerfQueriesEnable = false;

	// Load common settings
	iniFile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile::Section* interface = iniFile.GetOrCreateSection("Interface");
	bool bTmp;
	interface->Get("UsePanicHandlers", &bTmp, true);
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

	VerifyValidity();
}

void VideoConfig::GameIniLoad()
{
	bool gfx_override_exists = false;

	// XXX: Again, bad place to put OSD messages at (see delroth's comment above)
	// XXX: This will add an OSD message for each projection hack value... meh
#define CHECK_SETTING(section, key, var) do { \
		decltype(var) temp = var; \
		if (iniFile.GetIfExists(section, key, &var) && var != temp) { \
			std::string msg = StringFromFormat("Note: Option \"%s\" is overridden by game ini.", key); \
			OSD::AddMessage(msg, 7500); \
			gfx_override_exists = true; \
		} \
	} while (0)

	IniFile iniFile = SConfig::GetInstance().LoadGameIni();

	CHECK_SETTING("Video_Hardware", "VSync", bVSync);

	CHECK_SETTING("Video_Settings", "wideScreenHack", bWidescreenHack);
	CHECK_SETTING("Video_Settings", "AspectRatio", iAspectRatio);
	CHECK_SETTING("Video_Settings", "Crop", bCrop);
	CHECK_SETTING("Video_Settings", "UseXFB", bUseXFB);
	CHECK_SETTING("Video_Settings", "UseRealXFB", bUseRealXFB);
	CHECK_SETTING("Video_Settings", "SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);
	CHECK_SETTING("Video_Settings", "HiresTextures", bHiresTextures);
	CHECK_SETTING("Video_Settings", "ConvertHiresTextures", bConvertHiresTextures);
	CHECK_SETTING("Video_Settings", "CacheHiresTextures", bCacheHiresTextures);
	CHECK_SETTING("Video_Settings", "EnablePixelLighting", bEnablePixelLighting);
	CHECK_SETTING("Video_Settings", "FastDepthCalc", bFastDepthCalc);
	CHECK_SETTING("Video_Settings", "MSAA", iMultisamples);
	CHECK_SETTING("Video_Settings", "SSAA", bSSAA);

	int tmp = -9000;
	CHECK_SETTING("Video_Settings", "EFBScale", tmp); // integral
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

	CHECK_SETTING("Video_Settings", "DisableFog", bDisableFog);

	CHECK_SETTING("Video_Enhancements", "ForceFiltering", bForceFiltering);
	CHECK_SETTING("Video_Enhancements", "MaxAnisotropy", iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	CHECK_SETTING("Video_Enhancements", "PostProcessingShader", sPostProcessingShader);

	//these are not overrides, they are per-game settings, hence no warning
	IniFile::Section* stereoscopy = iniFile.GetOrCreateSection("Stereoscopy");
	for (size_t i = 0; i < oStereoPresets.size(); ++i)
	{
		stereoscopy->Get(StringFromFormat("StereoConvergencePercentage_%zu", i), &oStereoPresets[i].convergence, iStereoConvergencePercentage);
		stereoscopy->Get(StringFromFormat("StereoDepth_%zu", i), &oStereoPresets[i].depth, iStereoDepth);
	}
	stereoscopy->Get("StereoActivePreset", &iStereoActivePreset, 0);
	iStereoConvergencePercentage = oStereoPresets[iStereoActivePreset].convergence;
	iStereoDepth = oStereoPresets[iStereoActivePreset].depth;

	CHECK_SETTING("Video_Stereoscopy", "StereoMode", iStereoMode);
	CHECK_SETTING("Video_Stereoscopy", "StereoDepth", iStereoDepth);
	CHECK_SETTING("Video_Stereoscopy", "StereoConvergence", iStereoConvergence);
	CHECK_SETTING("Video_Stereoscopy", "StereoSwapEyes", bStereoSwapEyes);
	CHECK_SETTING("Video_Stereoscopy", "StereoEFBMonoDepth", bStereoEFBMonoDepth);
	CHECK_SETTING("Video_Stereoscopy", "StereoDepthPercentage", iStereoDepthPercentage);

	CHECK_SETTING("Video_Hacks", "EFBAccessEnable", bEFBAccessEnable);
	CHECK_SETTING("Video_Hacks", "BBoxEnable", bBBoxEnable);
	CHECK_SETTING("Video_Hacks", "ForceProgressive", bForceProgressive);
	CHECK_SETTING("Video_Hacks", "EFBToTextureEnable", bSkipEFBCopyToRam);
	CHECK_SETTING("Video_Hacks", "EFBScaledCopy", bCopyEFBScaled);
	CHECK_SETTING("Video_Hacks", "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

	CHECK_SETTING("Video", "ProjectionHack", iPhackvalue[0]);
	CHECK_SETTING("Video", "PH_SZNear", iPhackvalue[1]);
	CHECK_SETTING("Video", "PH_SZFar", iPhackvalue[2]);
	CHECK_SETTING("Video", "PH_ZNear", sPhackvalue[0]);
	CHECK_SETTING("Video", "PH_ZFar", sPhackvalue[1]);
	CHECK_SETTING("Video", "PerfQueriesEnable", bPerfQueriesEnable);

	if (gfx_override_exists)
		OSD::AddMessage("Warning: Opening the graphics configuration will reset settings and might cause issues!", 10000);
}

void VideoConfig::VerifyValidity()
{
	// TODO: Check iMaxAnisotropy value
	if (iAdapter < 0 || iAdapter > ((int)backend_info.Adapters.size() - 1))
		iAdapter = 0;

	if (std::find(backend_info.AAModes.begin(), backend_info.AAModes.end(), iMultisamples) == backend_info.AAModes.end())
		iMultisamples = 1;

	if (iStereoMode > 0)
	{
		if (!backend_info.bSupportsGeometryShaders)
		{
			OSD::AddMessage("Stereoscopic 3D isn't supported by your GPU, support for OpenGL 3.2 is required.", 10000);
			iStereoMode = 0;
		}

		if (bUseXFB && bUseRealXFB)
		{
			OSD::AddMessage("Stereoscopic 3D isn't supported with Real XFB, turning off stereoscopy.", 10000);
			iStereoMode = 0;
		}
	}
}

void VideoConfig::Save(const std::string& ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);

	IniFile::Section* hardware = iniFile.GetOrCreateSection("Hardware");
	hardware->Set("VSync", bVSync);
	hardware->Set("Adapter", iAdapter);

	IniFile::Section* settings = iniFile.GetOrCreateSection("Settings");
	settings->Set("AspectRatio", iAspectRatio);
	settings->Set("Crop", bCrop);
	settings->Set("wideScreenHack", bWidescreenHack);
	settings->Set("UseXFB", bUseXFB);
	settings->Set("UseRealXFB", bUseRealXFB);
	settings->Set("SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);
	settings->Set("ShowFPS", bShowFPS);
	settings->Set("LogRenderTimeToFile", bLogRenderTimeToFile);
	settings->Set("OverlayStats", bOverlayStats);
	settings->Set("OverlayProjStats", bOverlayProjStats);
	settings->Set("DumpTextures", bDumpTextures);
	settings->Set("HiresTextures", bHiresTextures);
	settings->Set("ConvertHiresTextures", bConvertHiresTextures);
	settings->Set("CacheHiresTextures", bCacheHiresTextures);
	settings->Set("DumpEFBTarget", bDumpEFBTarget);
	settings->Set("FreeLook", bFreeLook);
	settings->Set("UseFFV1", bUseFFV1);
	settings->Set("EnablePixelLighting", bEnablePixelLighting);
	settings->Set("FastDepthCalc", bFastDepthCalc);
	settings->Set("MSAA", iMultisamples);
	settings->Set("SSAA", bSSAA);
	settings->Set("EFBScale", iEFBScale);
	settings->Set("TexFmtOverlayEnable", bTexFmtOverlayEnable);
	settings->Set("TexFmtOverlayCenter", bTexFmtOverlayCenter);
	settings->Set("Wireframe", bWireFrame);
	settings->Set("DisableFog", bDisableFog);
	settings->Set("EnableShaderDebugging", bEnableShaderDebugging);
	settings->Set("BorderlessFullscreen", bBorderlessFullscreen);

	IniFile::Section* enhancements = iniFile.GetOrCreateSection("Enhancements");
	enhancements->Set("ForceFiltering", bForceFiltering);
	enhancements->Set("MaxAnisotropy", iMaxAnisotropy);
	enhancements->Set("PostProcessingShader", sPostProcessingShader);

	IniFile::Section* stereoscopy = iniFile.GetOrCreateSection("Stereoscopy");
	stereoscopy->Set("StereoMode", iStereoMode);
	stereoscopy->Set("StereoDepth", iStereoDepth);
	stereoscopy->Set("StereoConvergencePercentage", iStereoConvergencePercentage);
	stereoscopy->Set("StereoSwapEyes", bStereoSwapEyes);

	IniFile::Section* hacks = iniFile.GetOrCreateSection("Hacks");
	hacks->Set("EFBAccessEnable", bEFBAccessEnable);
	hacks->Set("BBoxEnable", bBBoxEnable);
	hacks->Set("ForceProgressive", bForceProgressive);
	hacks->Set("EFBToTextureEnable", bSkipEFBCopyToRam);
	hacks->Set("EFBScaledCopy", bCopyEFBScaled);
	hacks->Set("EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

	iniFile.Save(ini_file);
}

bool VideoConfig::IsVSync()
{
	return bVSync && !Core::GetIsFramelimiterTempDisabled();
}
