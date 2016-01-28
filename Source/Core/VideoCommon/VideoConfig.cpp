// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/OnionConfig.h"
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
}

void VideoConfig::Load()
{
	OnionConfig::OnionPetal* hardware = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Hardware");
	hardware->Get("VSync", &bVSync, 0);
	hardware->Get("Adapter", &iAdapter, 0);

	OnionConfig::OnionPetal* settings = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings");
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

	settings->Get("SWZComploc", &bZComploc, true);
	settings->Get("SWZFreeze", &bZFreeze, true);
	settings->Get("SWDumpObjects", &bDumpObjects, false);
	settings->Get("SWDumpTevStages", &bDumpTevStages, false);
	settings->Get("SWDumpTevTexFetches", &bDumpTevTextureFetches, false);
	settings->Get("SWDrawStart", &drawStart, 0);
	settings->Get("SWDrawEnd", &drawEnd, 100000);

	OnionConfig::OnionPetal* enhancements = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Enhancements");
	enhancements->Get("ForceFiltering", &bForceFiltering, 0);
	enhancements->Get("MaxAnisotropy", &iMaxAnisotropy, 0);  // NOTE - this is x in (1 << x)
	enhancements->Get("PostProcessingShader", &sPostProcessingShader, "");

	OnionConfig::OnionPetal* stereoscopy = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Stereoscopy");
	stereoscopy->Get("StereoMode", &iStereoMode, 0);
	stereoscopy->Get("StereoDepth", &iStereoDepth, 20);
	stereoscopy->Get("StereoConvergencePercentage", &iStereoConvergencePercentage, 100);
	stereoscopy->Get("StereoSwapEyes", &bStereoSwapEyes, false);

	OnionConfig::OnionPetal* hacks = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks");
	hacks->Get("EFBAccessEnable", &bEFBAccessEnable, true);
	hacks->Get("BBoxEnable", &bBBoxEnable, false);
	hacks->Get("ForceProgressive", &bForceProgressive, true);
	hacks->Get("EFBToTextureEnable", &bSkipEFBCopyToRam, true);
	hacks->Get("EFBScaledCopy", &bCopyEFBScaled, true);
	hacks->Get("EFBEmulateFormatChanges", &bEFBEmulateFormatChanges, false);

	OnionConfig::OnionPetal* stereo = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Stereoscopy");
	stereo->Get("StereoConvergence", &iStereoConvergence, 20);
	stereo->Get("StereoEFBMonoDepth", &bStereoEFBMonoDepth, false);
	stereo->Get("StereoDepthPercentage", &iStereoDepthPercentage, 100);

	// hacks which are disabled by default
	OnionConfig::OnionPetal* video = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_GFX, "Video");
	video->Get("ProjectionHack", &iPhackvalue[0], 0);
	video->Get("PH_SZNear", &iPhackvalue[1]);
	video->Get("PH_SZFar", &iPhackvalue[2]);
	video->Get("PH_ZNear", &sPhackvalue[0]);
	video->Get("PH_ZFar", &sPhackvalue[1]);
	video->Get("PerfQueriesEnable", &bPerfQueriesEnable, false);

	// Load common settings
	OnionConfig::OnionPetal* interface = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "Interface");
	bool bTmp;
	interface->Get("UsePanicHandlers", &bTmp, true);
	SetEnableAlert(bTmp);

	// XXX: Enable Callback handler

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

bool VideoConfig::IsVSync()
{
	return bVSync && !Core::GetIsThrottlerTempDisabled();
}
