// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
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

	// Needed for the first frame, I think
	fAspectRatioHackW = 1;
	fAspectRatioHackH = 1;

	// disable all features by default
	backend_info.APIType = API_NONE;
	backend_info.bUseRGBATextures = false;
	backend_info.bUseMinimalMipCount = false;
	backend_info.bSupports3DVision = false;
}

void VideoConfig::Load(const std::string& ini_file)
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
	iniFile.Get("Settings", "FastDepthCalc", &bFastDepthCalc, true);

	iniFile.Get("Settings", "MSAA", &iMultisampleMode, 0);
	iniFile.Get("Settings", "EFBScale", &iEFBScale, (int) SCALE_1X); // native

	iniFile.Get("Settings", "DstAlphaPass", &bDstAlphaPass, false);

	iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
	iniFile.Get("Settings", "WireFrame", &bWireFrame, 0);
	iniFile.Get("Settings", "DisableFog", &bDisableFog, 0);

	iniFile.Get("Settings", "OMPDecoder", &bOMPDecoder, false);

	iniFile.Get("Settings", "EnableShaderDebugging", &bEnableShaderDebugging, false);

	iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
	iniFile.Get("Enhancements", "MaxAnisotropy", &iMaxAnisotropy, 0);  // NOTE - this is x in (1 << x)
	iniFile.Get("Enhancements", "PostProcessingShader", &sPostProcessingShader, "");
	iniFile.Get("Enhancements", "Enable3dVision", &b3DVision, false);

	iniFile.Get("Hacks", "EFBAccessEnable", &bEFBAccessEnable, true);
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

void VideoConfig::GameIniLoad()
{
	bool gfx_override_exists = false;

	// XXX: Again, bad place to put OSD messages at (see delroth's comment above)
	// XXX: This will add an OSD message for each projection hack value... meh
#define CHECK_SETTING(section, key, var) do { \
		decltype(var) temp = var; \
		if (iniFile.GetIfExists(section, key, &var) && var != temp) { \
			char buf[256]; \
			snprintf(buf, sizeof(buf), "Note: Option \"%s\" is overridden by game ini.", key); \
			OSD::AddMessage(buf, 7500); \
			gfx_override_exists = true; \
		} \
	} while (0)

	IniFile iniFile = SConfig::GetInstance().m_LocalCoreStartupParameter.LoadGameIni();

	CHECK_SETTING("Video_Hardware", "VSync", bVSync);

	CHECK_SETTING("Video_Settings", "wideScreenHack", bWidescreenHack);
	CHECK_SETTING("Video_Settings", "AspectRatio", iAspectRatio);
	CHECK_SETTING("Video_Settings", "Crop", bCrop);
	CHECK_SETTING("Video_Settings", "UseXFB", bUseXFB);
	CHECK_SETTING("Video_Settings", "UseRealXFB", bUseRealXFB);
	CHECK_SETTING("Video_Settings", "SafeTextureCacheColorSamples", iSafeTextureCache_ColorSamples);
	CHECK_SETTING("Video_Settings", "HiresTextures", bHiresTextures);
	CHECK_SETTING("Video_Settings", "AnaglyphStereo", bAnaglyphStereo);
	CHECK_SETTING("Video_Settings", "AnaglyphStereoSeparation", iAnaglyphStereoSeparation);
	CHECK_SETTING("Video_Settings", "AnaglyphFocalAngle", iAnaglyphFocalAngle);
	CHECK_SETTING("Video_Settings", "EnablePixelLighting", bEnablePixelLighting);
	CHECK_SETTING("Video_Settings", "FastDepthCalc", bFastDepthCalc);
	CHECK_SETTING("Video_Settings", "MSAA", iMultisampleMode);
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

	CHECK_SETTING("Video_Settings", "DstAlphaPass", bDstAlphaPass);
	CHECK_SETTING("Video_Settings", "DisableFog", bDisableFog);
	CHECK_SETTING("Video_Settings", "OMPDecoder", bOMPDecoder);

	CHECK_SETTING("Video_Enhancements", "ForceFiltering", bForceFiltering);
	CHECK_SETTING("Video_Enhancements", "MaxAnisotropy", iMaxAnisotropy);  // NOTE - this is x in (1 << x)
	CHECK_SETTING("Video_Enhancements", "PostProcessingShader", sPostProcessingShader);
	CHECK_SETTING("Video_Enhancements", "Enable3dVision", b3DVision);

	CHECK_SETTING("Video_Hacks", "EFBAccessEnable", bEFBAccessEnable);
	CHECK_SETTING("Video_Hacks", "EFBCopyEnable", bEFBCopyEnable);
	CHECK_SETTING("Video_Hacks", "EFBToTextureEnable", bCopyEFBToTexture);
	CHECK_SETTING("Video_Hacks", "EFBScaledCopy", bCopyEFBScaled);
	CHECK_SETTING("Video_Hacks", "EFBCopyCacheEnable", bEFBCopyCacheEnable);
	CHECK_SETTING("Video_Hacks", "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

	CHECK_SETTING("Video", "ProjectionHack", iPhackvalue[0]);
	CHECK_SETTING("Video", "PH_SZNear", iPhackvalue[1]);
	CHECK_SETTING("Video", "PH_SZFar", iPhackvalue[2]);
	CHECK_SETTING("Video", "PH_ZNear", sPhackvalue[0]);
	CHECK_SETTING("Video", "PH_ZFar", sPhackvalue[1]);
	CHECK_SETTING("Video", "UseBBox", bUseBBox);
	CHECK_SETTING("Video", "PerfQueriesEnable", bPerfQueriesEnable);

	if (gfx_override_exists)
		OSD::AddMessage("Warning: Opening the graphics configuration will reset settings and might cause issues!", 10000);
}

void VideoConfig::VerifyValidity()
{
	// TODO: Check iMaxAnisotropy value
	if (iAdapter < 0 || iAdapter > ((int)backend_info.Adapters.size() - 1)) iAdapter = 0;
	if (iMultisampleMode < 0 || iMultisampleMode >= (int)backend_info.AAModes.size()) iMultisampleMode = 0;
	if (!backend_info.bSupports3DVision) b3DVision = false;
}

void VideoConfig::Save(const std::string& ini_file)
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
	iniFile.Set("Settings", "FastDepthCalc", bFastDepthCalc);

	iniFile.Set("Settings", "ShowEFBCopyRegions", bShowEFBCopyRegions);
	iniFile.Set("Settings", "MSAA", iMultisampleMode);
	iniFile.Set("Settings", "EFBScale", iEFBScale);
	iniFile.Set("Settings", "TexFmtOverlayEnable", bTexFmtOverlayEnable);
	iniFile.Set("Settings", "TexFmtOverlayCenter", bTexFmtOverlayCenter);
	iniFile.Set("Settings", "Wireframe", bWireFrame);
	iniFile.Set("Settings", "DstAlphaPass", bDstAlphaPass);
	iniFile.Set("Settings", "DisableFog", bDisableFog);

	iniFile.Set("Settings", "OMPDecoder", bOMPDecoder);

	iniFile.Set("Settings", "EnableShaderDebugging", bEnableShaderDebugging);

	iniFile.Set("Enhancements", "ForceFiltering", bForceFiltering);
	iniFile.Set("Enhancements", "MaxAnisotropy", iMaxAnisotropy);
	iniFile.Set("Enhancements", "PostProcessingShader", sPostProcessingShader);
	iniFile.Set("Enhancements", "Enable3dVision", b3DVision);

	iniFile.Set("Hacks", "EFBAccessEnable", bEFBAccessEnable);
	iniFile.Set("Hacks", "EFBCopyEnable", bEFBCopyEnable);
	iniFile.Set("Hacks", "EFBToTextureEnable", bCopyEFBToTexture);
	iniFile.Set("Hacks", "EFBScaledCopy", bCopyEFBScaled);
	iniFile.Set("Hacks", "EFBCopyCacheEnable", bEFBCopyCacheEnable);
	iniFile.Set("Hacks", "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

	iniFile.Set("Hardware", "Adapter", iAdapter);

	iniFile.Save(ini_file);
}

bool VideoConfig::IsVSync()
{
	return bVSync && !Core::GetIsFramelimiterTempDisabled();
}
