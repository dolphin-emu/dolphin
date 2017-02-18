// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/Config.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
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

  // Needed for the first frame, I think
  fAspectRatioHackW = 1;
  fAspectRatioHackH = 1;

  // disable all features by default
  backend_info.api_type = APIType::Nothing;
  backend_info.bSupportsExclusiveFullscreen = false;
  backend_info.bSupportsMultithreading = false;
  backend_info.bSupportsInternalResolutionFrameDumps = false;

  bEnableValidationLayer = false;
  bBackendMultithreading = true;
}

void VideoConfig::Load()
{
  Config::Section* hardware = Config::GetOrCreateSection(Config::System::GFX, "Hardware");
  hardware->Get("VSync", &bVSync, false);
  hardware->Get("Adapter", &iAdapter, 0);

  Config::Section* settings = Config::GetOrCreateSection(Config::System::GFX, "Settings");
  settings->Get("wideScreenHack", &bWidescreenHack, false);
  settings->Get("AspectRatio", &iAspectRatio, (int)ASPECT_AUTO);
  settings->Get("Crop", &bCrop, false);
  settings->Get("UseXFB", &bUseXFB, false);
  settings->Get("UseRealXFB", &bUseRealXFB, false);
  settings->Get("SafeTextureCacheColorSamples", &iSafeTextureCache_ColorSamples, 128);
  settings->Get("ShowFPS", &bShowFPS, false);
  settings->Get("ShowNetPlayPing", &bShowNetPlayPing, false);
  settings->Get("ShowNetPlayMessages", &bShowNetPlayMessages, false);
  settings->Get("LogRenderTimeToFile", &bLogRenderTimeToFile, false);
  settings->Get("OverlayStats", &bOverlayStats, false);
  settings->Get("OverlayProjStats", &bOverlayProjStats, false);
  settings->Get("DumpTextures", &bDumpTextures, false);
  settings->Get("HiresTextures", &bHiresTextures, false);
  settings->Get("ConvertHiresTextures", &bConvertHiresTextures, false);
  settings->Get("CacheHiresTextures", &bCacheHiresTextures, false);
  settings->Get("DumpEFBTarget", &bDumpEFBTarget, false);
  settings->Get("DumpFramesAsImages", &bDumpFramesAsImages, false);
  settings->Get("FreeLook", &bFreeLook, false);
  settings->Get("UseFFV1", &bUseFFV1, false);
  settings->Get("InternalResolutionFrameDumps", &bInternalResolutionFrameDumps, false);
  settings->Get("EnablePixelLighting", &bEnablePixelLighting, false);
  settings->Get("FastDepthCalc", &bFastDepthCalc, true);
  settings->Get("MSAA", &iMultisamples, 1);
  settings->Get("SSAA", &bSSAA, false);
  settings->Get("EFBScale", &iEFBScale, (int)SCALE_1X);  // native
  settings->Get("TexFmtOverlayEnable", &bTexFmtOverlayEnable, false);
  settings->Get("TexFmtOverlayCenter", &bTexFmtOverlayCenter, false);
  settings->Get("WireFrame", &bWireFrame, false);
  settings->Get("DisableFog", &bDisableFog, false);
  settings->Get("BorderlessFullscreen", &bBorderlessFullscreen, false);
  settings->Get("EnableValidationLayer", &bEnableValidationLayer, false);
  settings->Get("BackendMultithreading", &bBackendMultithreading, true);
  settings->Get("CommandBufferExecuteInterval", &iCommandBufferExecuteInterval, 100);

  settings->Get("SWZComploc", &bZComploc, true);
  settings->Get("SWZFreeze", &bZFreeze, true);
  settings->Get("SWDumpObjects", &bDumpObjects, false);
  settings->Get("SWDumpTevStages", &bDumpTevStages, false);
  settings->Get("SWDumpTevTexFetches", &bDumpTevTextureFetches, false);
  settings->Get("SWDrawStart", &drawStart, 0);
  settings->Get("SWDrawEnd", &drawEnd, 100000);

  Config::Section* enhancements = Config::GetOrCreateSection(Config::System::GFX, "Enhancements");
  enhancements->Get("ForceFiltering", &bForceFiltering, false);
  enhancements->Get("MaxAnisotropy", &iMaxAnisotropy, 0);  // NOTE - this is x in (1 << x)
  enhancements->Get("PostProcessingShader", &sPostProcessingShader, "");
  enhancements->Get("ForceTrueColor", &bForceTrueColor, true);

  Config::Section* stereoscopy = Config::GetOrCreateSection(Config::System::GFX, "Stereoscopy");
  stereoscopy->Get("StereoMode", &iStereoMode, 0);
  stereoscopy->Get("StereoDepth", &iStereoDepth, 20);
  stereoscopy->Get("StereoConvergencePercentage", &iStereoConvergencePercentage, 100);
  stereoscopy->Get("StereoSwapEyes", &bStereoSwapEyes, false);

  Config::Section* hacks = Config::GetOrCreateSection(Config::System::GFX, "Hacks");
  hacks->Get("EFBAccessEnable", &bEFBAccessEnable, true);
  hacks->Get("BBoxEnable", &bBBoxEnable, false);
  hacks->Get("ForceProgressive", &bForceProgressive, true);
  hacks->Get("EFBToTextureEnable", &bSkipEFBCopyToRam, true);
  hacks->Get("EFBScaledCopy", &bCopyEFBScaled, true);
  hacks->Get("EFBEmulateFormatChanges", &bEFBEmulateFormatChanges, false);

  Config::Section* stereo = Config::GetOrCreateSection(Config::System::GFX, "Stereoscopy");
  stereo->Get("StereoConvergence", &iStereoConvergence, 20);
  stereo->Get("StereoEFBMonoDepth", &bStereoEFBMonoDepth, false);
  stereo->Get("StereoDepthPercentage", &iStereoDepthPercentage, 100);

  // hacks which are disabled by default
  Config::Section* video = Config::GetOrCreateSection(Config::System::GFX, "Video");
  video->Get("ProjectionHack", &iPhackvalue[0], 0);
  video->Get("PH_SZNear", &iPhackvalue[1]);
  video->Get("PH_SZFar", &iPhackvalue[2]);
  video->Get("PH_ZNear", &sPhackvalue[0]);
  video->Get("PH_ZFar", &sPhackvalue[1]);
  video->Get("PerfQueriesEnable", &bPerfQueriesEnable, false);

  // Load common settings
  Config::Section* interface = Config::GetOrCreateSection(Config::System::Main, "Interface");
  bool bTmp;
  interface->Get("UsePanicHandlers", &bTmp, true);
  SetEnableAlert(bTmp);

  VerifyValidity();
}

void VideoConfig::VerifyValidity()
{
  // TODO: Check iMaxAnisotropy value
  if (iAdapter < 0 || iAdapter > ((int)backend_info.Adapters.size() - 1))
    iAdapter = 0;

  if (std::find(backend_info.AAModes.begin(), backend_info.AAModes.end(), iMultisamples) ==
      backend_info.AAModes.end())
    iMultisamples = 1;

  if (iStereoMode > 0)
  {
    if (!backend_info.bSupportsGeometryShaders)
    {
      OSD::AddMessage("Stereoscopic 3D isn't supported by your GPU, support "
                      "for OpenGL 3.2 is required.",
                      10000);
      iStereoMode = 0;
    }

    if (bUseXFB && bUseRealXFB)
    {
      OSD::AddMessage("Stereoscopic 3D isn't supported with Real XFB, turning "
                      "off stereoscopy.",
                      10000);
      iStereoMode = 0;
    }
  }
}

bool VideoConfig::IsVSync()
{
  return bVSync && !Core::GetIsThrottlerTempDisabled();
}
