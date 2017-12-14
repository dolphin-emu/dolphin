// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;
// VR settings need to be saved manually, with a prompt if settings are changed. This detects when
// they have changed.
VideoConfig g_SavedConfig;

void UpdateActiveConfig()
{
  if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
    Movie::SetGraphicsConfig();

  g_ActiveConfig = g_Config;
  if (g_has_hmd)
    g_ActiveConfig.bUseRealXFB = false;
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

  // VR
  fScale = 1.0f;
  fLeanBackAngle = 0;
  bStabilizePitch = true;
  bStabilizeRoll = true;
  bStabilizeYaw = false;
  bStabilizeX = false;
  bStabilizeY = false;
  bStabilizeZ = false;
  bPullUp20fps = false;
  bPullUp30fps = false;
  bPullUp60fps = false;
  bPullUpAuto = false;
  bOpcodeWarningDisable = false;
  bReplayVertexData = false;
  bPullUp20fpsTimewarp = false;
  bPullUp30fpsTimewarp = false;
  bPullUp60fpsTimewarp = false;
  bPullUpAutoTimewarp = false;
  bEnableVR = true;
  bAsynchronousTimewarp = false;
  bLowPersistence = true;
  bDynamicPrediction = true;
  bOrientationTracking = true;
  bMagYawCorrection = true;
  bPositionTracking = true;
  bChromatic = true;
  bTimewarp = true;
  bVignette = false;
  bNoRestore = false;
  bFlipVertical = false;
  bSRGB = false;
  bOverdrive = true;
  bHqDistortion = false;
  bDisableNearClipping = true;
  bAutoPairViveControllers = false;
  bShowHands = false;
  bShowFeet = false;
  bShowController = true;
  bShowLaserPointer = false;
  bShowAimRectangle = false;
  bShowHudBox = false;
  bShow2DBox = false;
  bShowSensorBar = false;
  bShowGameCamera = false;
  bShowGameFrustum = false;
  bShowTrackingCamera = false;
  bShowTrackingVolume = false;
  bShowBaseStation = false;

  bMotionSicknessAlways = false;
  bMotionSicknessFreelook = false;
  bMotionSickness2D = false;
  bMotionSicknessLeftStick = false;
  bMotionSicknessRightStick = false;
  bMotionSicknessDPad = false;
  iMotionSicknessMethod = 0;
  iMotionSicknessSkybox = 0;
  fMotionSicknessFOV = 45.0f;

  iVRPlayer = VR_PLAYER1;
  iVRPlayer2 = VR_PLAYER2;
  iMirrorPlayer = VR_PLAYER_DEFAULT;
  iMirrorStyle = VR_MIRROR_LEFT;
  fTimeWarpTweak = DEFAULT_VR_TIMEWARP_TWEAK;
  iExtraTimewarpedFrames = DEFAULT_VR_EXTRA_FRAMES;
  iExtraVideoLoops = DEFAULT_VR_EXTRA_VIDEO_LOOPS;
  iExtraVideoLoopsDivider = DEFAULT_VR_EXTRA_VIDEO_LOOPS_DIVIDER;

  sLeftTexture = "";
  sRightTexture = "";
  sGCLeftTexture = "";
  sGCRightTexture = "";

  fUnitsPerMetre = DEFAULT_VR_UNITS_PER_METRE;
  // in metres
  fFreeLookSensitivity = DEFAULT_VR_FREE_LOOK_SENSITIVITY;
  fHudDistance = DEFAULT_VR_HUD_DISTANCE;
  fHudThickness = DEFAULT_VR_HUD_THICKNESS;
  fHud3DCloser = DEFAULT_VR_HUD_3D_CLOSER;
  fAimDistance = DEFAULT_VR_AIM_DISTANCE;
  fScreenDistance = DEFAULT_VR_SCREEN_DISTANCE;
  fScreenHeight = DEFAULT_VR_SCREEN_HEIGHT;
  iMetroidPrime = 0;
  iTelescopeEye = 0;
  fTelescopeMaxFOV = 0;
  fMinFOV = DEFAULT_VR_MIN_FOV;
  fN64FOV = DEFAULT_VR_N64_FOV;
  fHudDespPosition0 = 0;
  fHudDespPosition1 = 0;
  fHudDespPosition2 = 0;
  Matrix33::LoadIdentity(matrixHudrot);
}

void VideoConfig::Load(const std::string& ini_file)
{
  IniFile iniFile;
  iniFile.Load(ini_file);

  IniFile::Section* hardware = iniFile.GetOrCreateSection("Hardware");
  if (ARBruteForcer::ch_bruteforce)
    bVSync = false;
  else
    hardware->Get("VSync", &bVSync, false);
  hardware->Get("Adapter", &iAdapter, 0);

  IniFile::Section* settings = iniFile.GetOrCreateSection("Settings");
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
  settings->Get("BitrateKbps", &iBitrateKbps, 2500);
  settings->Get("InternalResolutionFrameDumps", &bInternalResolutionFrameDumps, false);
  settings->Get("EnablePixelLighting", &bEnablePixelLighting, false);
  settings->Get("FastDepthCalc", &bFastDepthCalc, true);
  if (ARBruteForcer::ch_bruteforce)
  {
    iMultisamples = 0;
    bSSAA = false;
    iEFBScale = SCALE_1X;
  }
  else
  {
    settings->Get("MSAA", &iMultisamples, 0);
    settings->Get("SSAA", &bSSAA, false);
    settings->Get("EFBScale", &iEFBScale, (int)SCALE_1X);  // native
  }
  settings->Get("TexFmtOverlayEnable", &bTexFmtOverlayEnable, false);
  settings->Get("TexFmtOverlayCenter", &bTexFmtOverlayCenter, false);
  settings->Get("WireFrame", &bWireFrame, false);
  settings->Get("DisableFog", &bDisableFog, false);
  settings->Get("BorderlessFullscreen", &bBorderlessFullscreen, false);
  settings->Get("EnableValidationLayer", &bEnableValidationLayer, false);
  settings->Get("BackendMultithreading", &bBackendMultithreading, true);
  settings->Get("CommandBufferExecuteInterval", &iCommandBufferExecuteInterval, 100);
  settings->Get("ShaderCache", &bShaderCache, true);

  settings->Get("SWZComploc", &bZComploc, true);
  settings->Get("SWZFreeze", &bZFreeze, true);
  settings->Get("SWDumpObjects", &bDumpObjects, false);
  settings->Get("SWDumpTevStages", &bDumpTevStages, false);
  settings->Get("SWDumpTevTexFetches", &bDumpTevTextureFetches, false);
  settings->Get("SWDrawStart", &drawStart, 0);
  settings->Get("SWDrawEnd", &drawEnd, 100000);

  IniFile::Section* enhancements = iniFile.GetOrCreateSection("Enhancements");
  enhancements->Get("ForceFiltering", &bForceFiltering, false);
  enhancements->Get("MaxAnisotropy", &iMaxAnisotropy, 0);  // NOTE - this is x in (1 << x)
  enhancements->Get("PostProcessingShader", &sPostProcessingShader, "");
  enhancements->Get("ForceTrueColor", &bForceTrueColor, true);
  if ((g_has_rift || g_has_openvr) && backend_info.bSupportsGeometryShaders)
    iStereoMode = STEREO_OCULUS;

  IniFile::Section* stereoscopy = iniFile.GetOrCreateSection("Stereoscopy");
  stereoscopy->Get("StereoMode", &iStereoMode, 0);
  stereoscopy->Get("StereoDepth", &iStereoDepth, 20);
  stereoscopy->Get("StereoConvergencePercentage", &iStereoConvergencePercentage, 100);
  stereoscopy->Get("StereoSwapEyes", &bStereoSwapEyes, false);

  IniFile::Section* hacks = iniFile.GetOrCreateSection("Hacks");
  hacks->Get("EFBAccessEnable", &bEFBAccessEnable, true);
  hacks->Get("BBoxEnable", &bBBoxEnable, false);
  hacks->Get("ForceProgressive", &bForceProgressive, true);
  hacks->Get("EFBCopyEnable", &bEFBCopyEnable, true);
  hacks->Get("EFBCopyClearDisable", &bEFBCopyClearDisable, false);
  hacks->Get("EFBToTextureEnable", &bSkipEFBCopyToRam, true);
  hacks->Get("EFBScaledCopy", &bCopyEFBScaled, true);
  hacks->Get("EFBEmulateFormatChanges", &bEFBEmulateFormatChanges, false);

  // hacks which are disabled by default
  iPhackvalue[0] = 0;
  bPerfQueriesEnable = false;

  LoadVR(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");

  // Load common settings
  iniFile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  IniFile::Section* interface = iniFile.GetOrCreateSection("Interface");
  bool bTmp;
  interface->Get("UsePanicHandlers", &bTmp, true);
  SetEnableAlert(bTmp);

  VerifyValidity();
}

void VideoConfig::LoadVR(const std::string& ini_file)
{
  IniFile iniFile;
  iniFile.Load(ini_file);
  bool bNoMirrorToWindow;

  IniFile::Section* vr = iniFile.GetOrCreateSection("VR");
  vr->Get("Scale", &fScale, 1.0f);
  vr->Get("FreeLookSensitivity", &fFreeLookSensitivity, DEFAULT_VR_FREE_LOOK_SENSITIVITY);
  vr->Get("LeanBackAngle", &fLeanBackAngle, 0);
  vr->Get("EnableVR", &bEnableVR, true);
  vr->Get("LowPersistence", &bLowPersistence, true);
  vr->Get("DynamicPrediction", &bDynamicPrediction, true);
  vr->Get("NoMirrorToWindow", &bNoMirrorToWindow, false);
  vr->Get("OrientationTracking", &bOrientationTracking, true);
  vr->Get("MagYawCorrection", &bMagYawCorrection, true);
  vr->Get("PositionTracking", &bPositionTracking, true);
  vr->Get("Chromatic", &bChromatic, true);
  vr->Get("Timewarp", &bTimewarp, true);
  vr->Get("Vignette", &bVignette, false);
  vr->Get("NoRestore", &bNoRestore, false);
  vr->Get("FlipVertical", &bFlipVertical, false);
  vr->Get("sRGB", &bSRGB, false);
  vr->Get("Overdrive", &bOverdrive, true);
  vr->Get("HQDistortion", &bHqDistortion, false);
  vr->Get("DisableNearClipping", &bDisableNearClipping, true);
  vr->Get("AutoPairViveControllers", &bAutoPairViveControllers, false);
  vr->Get("ShowHands", &bShowHands, false);
  vr->Get("ShowFeet", &bShowFeet, false);
  vr->Get("ShowController", &bShowController, false);
  vr->Get("ShowLaserPointer", &bShowLaserPointer, false);
  vr->Get("ShowAimRectangle", &bShowAimRectangle, false);
  vr->Get("ShowHudBox", &bShowHudBox, false);
  vr->Get("Show2DScreenBox", &bShow2DBox, false);
  vr->Get("ShowSensorBar", &bShowSensorBar, false);
  vr->Get("ShowGameCamera", &bShowGameCamera, false);
  vr->Get("ShowGameFrustum", &bShowGameFrustum, false);
  vr->Get("ShowTrackingCamera", &bShowTrackingCamera, false);
  vr->Get("ShowTrackingVolume", &bShowTrackingVolume, false);
  vr->Get("ShowBaseStation", &bShowBaseStation, false);
  vr->Get("MotionSicknessAlways", &bMotionSicknessAlways, false);
  vr->Get("MotionSicknessFreelook", &bMotionSicknessFreelook, false);
  vr->Get("MotionSickness2D", &bMotionSickness2D, false);
  vr->Get("MotionSicknessLeftStick", &bMotionSicknessLeftStick, false);
  vr->Get("MotionSicknessRightStick", &bMotionSicknessRightStick, false);
  vr->Get("MotionSicknessDPad", &bMotionSicknessDPad, false);
  vr->Get("MotionSicknessIR", &bMotionSicknessIR, false);
  vr->Get("MotionSicknessMethod", &iMotionSicknessMethod, 0);
  vr->Get("MotionSicknessSkybox", &iMotionSicknessSkybox, 0);
  vr->Get("MotionSicknessFOV", &fMotionSicknessFOV, DEFAULT_VR_MOTION_SICKNESS_FOV);
  vr->Get("Player", &iVRPlayer, 0);
  vr->Get("Player2", &iVRPlayer2, 1);
  vr->Get("MirrorPlayer", &iMirrorPlayer, VR_PLAYER_DEFAULT);
  iMirrorStyle = bNoMirrorToWindow ? VR_MIRROR_DISABLED : VR_MIRROR_LEFT;
  vr->Get("MirrorStyle", &iMirrorStyle, iMirrorStyle);
  vr->Get("TimewarpTweak", &fTimeWarpTweak, DEFAULT_VR_TIMEWARP_TWEAK);
  vr->Get("NumExtraFrames", &iExtraTimewarpedFrames, DEFAULT_VR_EXTRA_FRAMES);
  vr->Get("NumExtraVideoLoops", &iExtraVideoLoops, DEFAULT_VR_EXTRA_VIDEO_LOOPS);
  vr->Get("NumExtraVideoLoopsDivider", &iExtraVideoLoopsDivider,
          DEFAULT_VR_EXTRA_VIDEO_LOOPS_DIVIDER);
  vr->Get("StabilizeRoll", &bStabilizeRoll, true);
  vr->Get("StabilizePitch", &bStabilizePitch, true);
  vr->Get("StabilizeYaw", &bStabilizeYaw, false);
  vr->Get("StabilizeX", &bStabilizeX, false);
  vr->Get("StabilizeY", &bStabilizeY, false);
  vr->Get("StabilizeZ", &bStabilizeZ, false);
  vr->Get("Keyhole", &bKeyhole, false);
  vr->Get("KeyholeWidth", &fKeyholeWidth, 45.0f);
  vr->Get("KeyholeSnap", &bKeyholeSnap, false);
  vr->Get("KeyholeSnapSize", &fKeyholeSnapSize, 30.0f);
  vr->Get("PullUp20fps", &bPullUp20fps, false);
  vr->Get("PullUp30fps", &bPullUp30fps, false);
  vr->Get("PullUp60fps", &bPullUp60fps, false);
  vr->Get("PullUpAuto", &bPullUpAuto, false);
  vr->Get("OpcodeReplay", &bOpcodeReplay, false);
  vr->Get("OpcodeWarningDisable", &bOpcodeWarningDisable, false);
  vr->Get("ReplayVertexData", &bReplayVertexData, false);
  vr->Get("ReplayOtherData", &bReplayOtherData, false);
  vr->Get("PullUp20fpsTimewarp", &bPullUp20fpsTimewarp, false);
  vr->Get("PullUp30fpsTimewarp", &bPullUp30fpsTimewarp, false);
  vr->Get("PullUp60fpsTimewarp", &bPullUp60fpsTimewarp, false);
  vr->Get("PullUpAutoTimewarp", &bPullUpAutoTimewarp, false);
  vr->Get("SynchronousTimewarp", &bSynchronousTimewarp, false);
  vr->Get("LeftTexture", &sLeftTexture);
  vr->Get("RightTexture", &sRightTexture);
  vr->Get("GCLeftTexture", &sGCLeftTexture);
  vr->Get("GCRightTexture", &sGCRightTexture);
}

void VideoConfig::GameIniLoad()
{
  bool gfx_override_exists = false;

// XXX: Again, bad place to put OSD messages at (see delroth's comment above)
// XXX: This will add an OSD message for each projection hack value... meh
#define CHECK_SETTING(section, key, var)                                                           \
  do                                                                                               \
  {                                                                                                \
    decltype(var) temp = var;                                                                      \
    if (iniFile.GetIfExists(section, key, &var, var) && var != temp)                               \
    {                                                                                              \
      std::string msg = StringFromFormat("Note: Option \"%s\" is overridden by game ini.", key);   \
      OSD::AddMessage(msg, 7500);                                                                  \
      gfx_override_exists = true;                                                                  \
    }                                                                                              \
  } while (0)

  IniFile iniFile = SConfig::GetInstance().LoadGameIni();

  if (g_has_hmd)
  {
    iniFile.OverrideSectionWithSection("Video_Settings", "Video_Settings_VR");
    iniFile.OverrideSectionWithSection("Video_Hardware", "Video_Hardware_VR");
    iniFile.OverrideSectionWithSection("Video_Enhancements", "Video_Enhancements_VR");
    iniFile.OverrideSectionWithSection("Video_Hacks", "Video_Hacks_VR");
    iniFile.OverrideSectionWithSection("Video", "Video_VR");
  }

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
  CHECK_SETTING("Video_Settings", "ForceTrueColor", bForceTrueColor);

  int tmp = -9000;
  CHECK_SETTING("Video_Settings", "EFBScale", tmp);  // integral
  if (tmp != -9000)
  {
    if (tmp != SCALE_FORCE_INTEGRAL)
    {
      iEFBScale = tmp;
    }
    else  // Round down to multiple of native IR
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
  CHECK_SETTING("Video_Settings", "BackendMultithreading", bBackendMultithreading);
  CHECK_SETTING("Video_Settings", "CommandBufferExecuteInterval", iCommandBufferExecuteInterval);

  CHECK_SETTING("Video_Enhancements", "ForceFiltering", bForceFiltering);
  CHECK_SETTING("Video_Enhancements", "MaxAnisotropy",
                iMaxAnisotropy);  // NOTE - this is x in (1 << x)
  CHECK_SETTING("Video_Enhancements", "PostProcessingShader", sPostProcessingShader);

  // These are not overrides, they are per-game stereoscopy parameters, hence no warning
  iniFile.GetIfExists("Video_Stereoscopy", "StereoConvergence", &iStereoConvergence, 20);
  iniFile.GetIfExists("Video_Stereoscopy", "StereoEFBMonoDepth", &bStereoEFBMonoDepth, false);
  iniFile.GetIfExists("Video_Stereoscopy", "StereoDepthPercentage", &iStereoDepthPercentage, 100);

  CHECK_SETTING("Video_Stereoscopy", "StereoMode", iStereoMode);
  CHECK_SETTING("Video_Stereoscopy", "StereoDepth", iStereoDepth);
  CHECK_SETTING("Video_Stereoscopy", "StereoSwapEyes", bStereoSwapEyes);

  CHECK_SETTING("Video_Hacks", "EFBAccessEnable", bEFBAccessEnable);
  CHECK_SETTING("Video_Hacks", "BBoxEnable", bBBoxEnable);
  CHECK_SETTING("Video_Hacks", "ForceProgressive", bForceProgressive);
  CHECK_SETTING("Video_Hacks", "EFBCopyEnable", bEFBCopyEnable);
  CHECK_SETTING("Video_Hacks", "EFBCopyClearDisable", bEFBCopyClearDisable);
  CHECK_SETTING("Video_Hacks", "EFBToTextureEnable", bSkipEFBCopyToRam);
  CHECK_SETTING("Video_Hacks", "EFBScaledCopy", bCopyEFBScaled);
  CHECK_SETTING("Video_Hacks", "EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

  CHECK_SETTING("Video", "ProjectionHack", iPhackvalue[0]);
  CHECK_SETTING("Video", "PH_SZNear", iPhackvalue[1]);
  CHECK_SETTING("Video", "PH_SZFar", iPhackvalue[2]);
  CHECK_SETTING("Video", "PH_ZNear", sPhackvalue[0]);
  CHECK_SETTING("Video", "PH_ZFar", sPhackvalue[1]);
  CHECK_SETTING("Video", "PerfQueriesEnable", bPerfQueriesEnable);

  fUnitsPerMetre = DEFAULT_VR_UNITS_PER_METRE;
  fHudDistance = DEFAULT_VR_HUD_DISTANCE;
  fHudThickness = DEFAULT_VR_HUD_THICKNESS;
  fHud3DCloser = DEFAULT_VR_HUD_3D_CLOSER;
  fCameraForward = DEFAULT_VR_CAMERA_FORWARD;
  fCameraPitch = DEFAULT_VR_CAMERA_PITCH;
  fAimDistance = DEFAULT_VR_AIM_DISTANCE;
  fMinFOV = DEFAULT_VR_MIN_FOV;
  fN64FOV = DEFAULT_VR_N64_FOV;
  fScreenHeight = DEFAULT_VR_SCREEN_HEIGHT;
  fScreenDistance = DEFAULT_VR_SCREEN_DISTANCE;
  fScreenThickness = DEFAULT_VR_HUD_THICKNESS;
  fScreenRight = DEFAULT_VR_SCREEN_RIGHT;
  fScreenUp = DEFAULT_VR_SCREEN_UP;
  fScreenPitch = DEFAULT_VR_SCREEN_PITCH;
  fHudDespPosition0 = 0;
  fHudDespPosition1 = 0;
  fHudDespPosition2 = 0;

  fReadPitch = 0;
  iCameraMinPoly = 0;
  bDisable3D = false;
  bHudFullscreen = false;
  bHudOnTop = false;
  bDontClearScreen = false;
  bCanReadCameraAngles = false;
  bDetectSkybox = false;
  iSelectedLayer = -2;
  iFlashState = 0;

  CHECK_SETTING("VR", "Disable3D", bDisable3D);
  CHECK_SETTING("VR", "HudFullscreen", bHudFullscreen);
  CHECK_SETTING("VR", "HudOnTop", bHudOnTop);
  CHECK_SETTING("VR", "DontClearScreen", bDontClearScreen);
  CHECK_SETTING("VR", "CanReadCameraAngles", bCanReadCameraAngles);
  CHECK_SETTING("VR", "DetectSkybox", bDetectSkybox);
  CHECK_SETTING("VR", "UnitsPerMetre", fUnitsPerMetre);
  CHECK_SETTING("VR", "HudThickness", fHudThickness);
  CHECK_SETTING("VR", "HudDistance", fHudDistance);
  CHECK_SETTING("VR", "Hud3DCloser", fHud3DCloser);
  CHECK_SETTING("VR", "CameraForward", fCameraForward);
  CHECK_SETTING("VR", "CameraPitch", fCameraPitch);
  CHECK_SETTING("VR", "AimDistance", fAimDistance);
  CHECK_SETTING("VR", "MinFOV", fMinFOV);
  CHECK_SETTING("VR", "N64FOV", fN64FOV);
  CHECK_SETTING("VR", "ScreenHeight", fScreenHeight);
  CHECK_SETTING("VR", "ScreenThickness", fScreenThickness);
  CHECK_SETTING("VR", "ScreenDistance", fScreenDistance);
  CHECK_SETTING("VR", "ScreenRight", fScreenRight);
  CHECK_SETTING("VR", "ScreenUp", fScreenUp);
  CHECK_SETTING("VR", "ScreenPitch", fScreenPitch);
  CHECK_SETTING("VR", "MetroidPrime", iMetroidPrime);
  CHECK_SETTING("VR", "TelescopeEye", iTelescopeEye);
  CHECK_SETTING("VR", "TelescopeFOV", fTelescopeMaxFOV);
  CHECK_SETTING("VR", "ReadPitch", fReadPitch);
  CHECK_SETTING("VR", "CameraMinPoly", iCameraMinPoly);
  CHECK_SETTING("VR", "HudDespPosition0", fHudDespPosition0);
  CHECK_SETTING("VR", "HudDespPosition1", fHudDespPosition1);
  CHECK_SETTING("VR", "HudDespPosition2", fHudDespPosition2);

  NOTICE_LOG(VR, "%f units per metre (each unit is %f cm), HUD is %fm away and %fm thick",
             fUnitsPerMetre, 100.0f / fUnitsPerMetre, fHudDistance, fHudThickness);

  g_SavedConfig = *this;
  if (gfx_override_exists)
    OSD::AddMessage(
        "Warning: Opening the graphics configuration will reset settings and might cause issues!",
        10000);
}

void VideoConfig::GameIniSave()
{
  // Load game ini
  IniFile GameIniDefault, GameIniLocal;
  GameIniDefault = SConfig::GetInstance().LoadDefaultGameIni();
  GameIniLocal = SConfig::GetInstance().LoadLocalGameIni();

#define SAVE_IF_NOT_DEFAULT(section, key, val, def)                                                \
  do                                                                                               \
  {                                                                                                \
    if (GameIniDefault.Exists((section), (key)))                                                   \
    {                                                                                              \
      std::remove_reference<decltype((val))>::type tmp__;                                          \
      GameIniDefault.GetOrCreateSection((section))->Get((key), &tmp__);                            \
      if ((val) != tmp__)                                                                          \
        GameIniLocal.GetOrCreateSection((section))->Set((key), (val));                             \
      else                                                                                         \
        GameIniLocal.DeleteKey((section), (key));                                                  \
    }                                                                                              \
    else if ((val) != (def))                                                                       \
      GameIniLocal.GetOrCreateSection((section))->Set((key), (val));                               \
    else                                                                                           \
      GameIniLocal.DeleteKey((section), (key));                                                    \
  } while (0)

  SAVE_IF_NOT_DEFAULT("VR", "Disable3D", bDisable3D, false);
  SAVE_IF_NOT_DEFAULT("VR", "UnitsPerMetre", (float)fUnitsPerMetre, DEFAULT_VR_UNITS_PER_METRE);
  SAVE_IF_NOT_DEFAULT("VR", "HudFullscreen", bHudFullscreen, false);
  SAVE_IF_NOT_DEFAULT("VR", "HudOnTop", bHudOnTop, false);
  SAVE_IF_NOT_DEFAULT("VR", "DontClearScreen", bDontClearScreen, false);
  SAVE_IF_NOT_DEFAULT("VR", "CanReadCameraAngles", bCanReadCameraAngles, false);
  SAVE_IF_NOT_DEFAULT("VR", "DetectSkybox", bDetectSkybox, false);
  SAVE_IF_NOT_DEFAULT("VR", "HudDistance", (float)fHudDistance, DEFAULT_VR_HUD_DISTANCE);
  SAVE_IF_NOT_DEFAULT("VR", "HudThickness", (float)fHudThickness, DEFAULT_VR_HUD_THICKNESS);
  SAVE_IF_NOT_DEFAULT("VR", "Hud3DCloser", (float)fHud3DCloser, DEFAULT_VR_HUD_3D_CLOSER);
  SAVE_IF_NOT_DEFAULT("VR", "CameraForward", (float)fCameraForward, DEFAULT_VR_CAMERA_FORWARD);
  SAVE_IF_NOT_DEFAULT("VR", "CameraPitch", (float)fCameraPitch, DEFAULT_VR_CAMERA_PITCH);
  SAVE_IF_NOT_DEFAULT("VR", "AimDistance", (float)fAimDistance, DEFAULT_VR_AIM_DISTANCE);
  SAVE_IF_NOT_DEFAULT("VR", "MinFOV", (float)fMinFOV, DEFAULT_VR_MIN_FOV);
  SAVE_IF_NOT_DEFAULT("VR", "N64FOV", (float)fN64FOV, DEFAULT_VR_N64_FOV);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenHeight", (float)fScreenHeight, DEFAULT_VR_SCREEN_HEIGHT);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenDistance", (float)fScreenDistance, DEFAULT_VR_SCREEN_DISTANCE);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenThickness", (float)fScreenThickness,
                      DEFAULT_VR_SCREEN_THICKNESS);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenUp", (float)fScreenUp, DEFAULT_VR_SCREEN_UP);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenRight", (float)fScreenRight, DEFAULT_VR_SCREEN_RIGHT);
  SAVE_IF_NOT_DEFAULT("VR", "ScreenPitch", (float)fScreenPitch, DEFAULT_VR_SCREEN_PITCH);
  SAVE_IF_NOT_DEFAULT("VR", "ReadPitch", (float)fReadPitch, 0.0f);
  SAVE_IF_NOT_DEFAULT("VR", "HudDespPosition0", (float)fHudDespPosition0, 0.0f);
  SAVE_IF_NOT_DEFAULT("VR", "HudDespPosition1", (float)fHudDespPosition1, 0.0f);
  SAVE_IF_NOT_DEFAULT("VR", "HudDespPosition2", (float)fHudDespPosition2, 0.0f);
  SAVE_IF_NOT_DEFAULT("VR", "CameraMinPoly", (int)iCameraMinPoly, 0);

  GameIniLocal.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() +
                    ".ini");
  g_SavedConfig = *this;
}

void VideoConfig::GameIniReset()
{
  // Load game ini
  IniFile GameIniDefault = SConfig::GetInstance().LoadDefaultGameIni();

#define LOAD_DEFAULT(section, key, var, def)                                                       \
  do                                                                                               \
  {                                                                                                \
    decltype(var) temp = var;                                                                      \
    if (GameIniDefault.GetIfExists(section, key, &var))                                            \
      var = temp;                                                                                  \
    else                                                                                           \
      var = def;                                                                                   \
  } while (0)

  LOAD_DEFAULT("VR", "Disable3D", bDisable3D, false);
  LOAD_DEFAULT("VR", "UnitsPerMetre", fUnitsPerMetre, DEFAULT_VR_UNITS_PER_METRE);
  LOAD_DEFAULT("VR", "HudFullscreen", bHudFullscreen, false);
  LOAD_DEFAULT("VR", "HudOnTop", bHudOnTop, false);
  LOAD_DEFAULT("VR", "DontClearScreen", bDontClearScreen, false);
  LOAD_DEFAULT("VR", "CanReadCameraAngles", bCanReadCameraAngles, false);
  LOAD_DEFAULT("VR", "DetectSkybox", bDetectSkybox, false);
  LOAD_DEFAULT("VR", "HudDistance", fHudDistance, DEFAULT_VR_HUD_DISTANCE);
  LOAD_DEFAULT("VR", "HudThickness", fHudThickness, DEFAULT_VR_HUD_THICKNESS);
  LOAD_DEFAULT("VR", "Hud3DCloser", fHud3DCloser, DEFAULT_VR_HUD_3D_CLOSER);
  LOAD_DEFAULT("VR", "CameraForward", fCameraForward, DEFAULT_VR_CAMERA_FORWARD);
  LOAD_DEFAULT("VR", "CameraPitch", fCameraPitch, DEFAULT_VR_CAMERA_PITCH);
  LOAD_DEFAULT("VR", "AimDistance", fAimDistance, DEFAULT_VR_AIM_DISTANCE);
  LOAD_DEFAULT("VR", "MinFOV", fMinFOV, DEFAULT_VR_MIN_FOV);
  LOAD_DEFAULT("VR", "N64FOV", fN64FOV, DEFAULT_VR_N64_FOV);
  LOAD_DEFAULT("VR", "ScreenHeight", fScreenHeight, DEFAULT_VR_SCREEN_HEIGHT);
  LOAD_DEFAULT("VR", "ScreenDistance", fScreenDistance, DEFAULT_VR_SCREEN_DISTANCE);
  LOAD_DEFAULT("VR", "ScreenThickness", fScreenThickness, DEFAULT_VR_SCREEN_THICKNESS);
  LOAD_DEFAULT("VR", "ScreenUp", fScreenUp, DEFAULT_VR_SCREEN_UP);
  LOAD_DEFAULT("VR", "ScreenRight", fScreenRight, DEFAULT_VR_SCREEN_RIGHT);
  LOAD_DEFAULT("VR", "ScreenPitch", fScreenPitch, DEFAULT_VR_SCREEN_PITCH);
  LOAD_DEFAULT("VR", "ReadPitch", fReadPitch, 0.0f);
  LOAD_DEFAULT("VR", "HudDespPosition0", fHudDespPosition0, 0.0f);
  LOAD_DEFAULT("VR", "HudDespPosition1", fHudDespPosition1, 0.0f);
  LOAD_DEFAULT("VR", "HudDespPosition2", fHudDespPosition2, 0.0f);
  LOAD_DEFAULT("VR", "CameraMinPoly", iCameraMinPoly, 0);
}

void VideoConfig::VerifyValidity()
{
  // TODO: Check iMaxAnisotropy value
  if (iAdapter < 0 || iAdapter > ((int)backend_info.Adapters.size() - 1))
    iAdapter = 0;

  if (std::find(backend_info.AAModes.begin(), backend_info.AAModes.end(), iMultisamples) ==
      backend_info.AAModes.end())
    iMultisamples = 1;

  if (g_has_rift || g_has_openvr)
    iStereoMode = STEREO_OCULUS;
  else if (g_has_vr920)
    iStereoMode = STEREO_VR920;
  else if (iStereoMode == STEREO_OCULUS || iStereoMode == STEREO_VR920)
    iStereoMode = 0;
  if (iStereoMode > 0)
  {
    if (!backend_info.bSupportsGeometryShaders)
    {
      PanicAlertT(
          "Stereoscopic 3D isn't supported by your GPU, support for OpenGL 3.2 is required.",
          10000);
      iStereoMode = 0;
    }

    if (bUseXFB && bUseRealXFB && !g_has_hmd)
    {
      OSD::AddMessage("Stereoscopic 3D isn't supported with Real XFB, turning off stereoscopy.",
                      10000);
      iStereoMode = 0;
    }
  }
}

void VideoConfig::Save(const std::string& ini_file)
{
  IniFile iniFile;
  iniFile.Load(ini_file);

  IniFile::Section* hardware = iniFile.GetOrCreateSection("Hardware");
  if (!ARBruteForcer::ch_dont_save_settings)
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
  settings->Set("ShowNetPlayPing", bShowNetPlayPing);
  settings->Set("ShowNetPlayMessages", bShowNetPlayMessages);
  settings->Set("LogRenderTimeToFile", bLogRenderTimeToFile);
  settings->Set("OverlayStats", bOverlayStats);
  settings->Set("OverlayProjStats", bOverlayProjStats);
  settings->Set("DumpTextures", bDumpTextures);
  settings->Set("HiresTextures", bHiresTextures);
  settings->Set("ConvertHiresTextures", bConvertHiresTextures);
  settings->Set("CacheHiresTextures", bCacheHiresTextures);
  settings->Set("DumpEFBTarget", bDumpEFBTarget);
  settings->Set("DumpFramesAsImages", bDumpFramesAsImages);
  settings->Set("FreeLook", bFreeLook);
  settings->Set("UseFFV1", bUseFFV1);
  settings->Set("BitrateKbps", iBitrateKbps);
  settings->Set("InternalResolutionFrameDumps", bInternalResolutionFrameDumps);
  settings->Set("EnablePixelLighting", bEnablePixelLighting);
  settings->Set("FastDepthCalc", bFastDepthCalc);
  if (!ARBruteForcer::ch_dont_save_settings)
  {
    settings->Set("MSAA", iMultisamples);
    settings->Set("SSAA", bSSAA);
    settings->Set("EFBScale", iEFBScale);
  }
  settings->Set("TexFmtOverlayEnable", bTexFmtOverlayEnable);
  settings->Set("TexFmtOverlayCenter", bTexFmtOverlayCenter);
  settings->Set("Wireframe", bWireFrame);
  settings->Set("DisableFog", bDisableFog);
  settings->Set("BorderlessFullscreen", bBorderlessFullscreen);
  settings->Set("EnableValidationLayer", bEnableValidationLayer);
  settings->Set("BackendMultithreading", bBackendMultithreading);
  settings->Set("CommandBufferExecuteInterval", iCommandBufferExecuteInterval);
  settings->Set("ShaderCache", bShaderCache);

  settings->Set("SWZComploc", bZComploc);
  settings->Set("SWZFreeze", bZFreeze);
  settings->Set("SWDumpObjects", bDumpObjects);
  settings->Set("SWDumpTevStages", bDumpTevStages);
  settings->Set("SWDumpTevTexFetches", bDumpTevTextureFetches);
  settings->Set("SWDrawStart", drawStart);
  settings->Set("SWDrawEnd", drawEnd);

  IniFile::Section* enhancements = iniFile.GetOrCreateSection("Enhancements");
  enhancements->Set("ForceFiltering", bForceFiltering);
  enhancements->Set("MaxAnisotropy", iMaxAnisotropy);
  enhancements->Set("PostProcessingShader", sPostProcessingShader);
  enhancements->Set("ForceTrueColor", bForceTrueColor);

  IniFile::Section* stereoscopy = iniFile.GetOrCreateSection("Stereoscopy");
  stereoscopy->Set("StereoMode", iStereoMode);
  stereoscopy->Set("StereoDepth", iStereoDepth);
  stereoscopy->Set("StereoConvergencePercentage", iStereoConvergencePercentage);
  stereoscopy->Set("StereoSwapEyes", bStereoSwapEyes);

  IniFile::Section* hacks = iniFile.GetOrCreateSection("Hacks");
  hacks->Set("EFBAccessEnable", bEFBAccessEnable);
  hacks->Set("BBoxEnable", bBBoxEnable);
  hacks->Set("ForceProgressive", bForceProgressive);
  hacks->Set("EFBCopyEnable", bEFBCopyEnable);
  hacks->Set("EFBCopyClearDisable", bEFBCopyClearDisable);
  hacks->Set("EFBToTextureEnable", bSkipEFBCopyToRam);
  hacks->Set("EFBScaledCopy", bCopyEFBScaled);
  hacks->Set("EFBEmulateFormatChanges", bEFBEmulateFormatChanges);

  SaveVR(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
  iniFile.Save(ini_file);
}

void VideoConfig::SaveVR(const std::string& ini_file)
{
  IniFile iniFile;
  iniFile.Load(ini_file);

  IniFile::Section* vr = iniFile.GetOrCreateSection("VR");
  vr->Set("Scale", fScale);
  vr->Set("FreeLookSensitivity", fFreeLookSensitivity);
  vr->Set("LeanBackAngle", fLeanBackAngle);
  vr->Set("EnableVR", bEnableVR);
  vr->Set("LowPersistence", bLowPersistence);
  vr->Set("DynamicPrediction", bDynamicPrediction);
  vr->Set("NoMirrorToWindow",
          iMirrorPlayer == VR_PLAYER_NONE || iMirrorStyle == VR_MIRROR_DISABLED);
  vr->Set("OrientationTracking", bOrientationTracking);
  vr->Set("MagYawCorrection", bMagYawCorrection);
  vr->Set("PositionTracking", bPositionTracking);
  vr->Set("Chromatic", bChromatic);
  vr->Set("Timewarp", bTimewarp);
  vr->Set("Vignette", bVignette);
  vr->Set("NoRestore", bNoRestore);
  vr->Set("FlipVertical", bFlipVertical);
  vr->Set("sRGB", bSRGB);
  vr->Set("Overdrive", bOverdrive);
  vr->Set("HQDistortion", bHqDistortion);
  vr->Set("DisableNearClipping", bDisableNearClipping);
  vr->Set("AutoPairViveControllers", bAutoPairViveControllers);
  vr->Set("ShowHands", bShowHands);
  vr->Set("ShowFeet", bShowFeet);
  vr->Set("ShowController", bShowController);
  vr->Set("ShowLaserPointer", bShowLaserPointer);
  vr->Set("ShowAimRectangle", bShowAimRectangle);
  vr->Set("ShowHudBox", bShowHudBox);
  vr->Set("Show2DScreenBox", bShow2DBox);
  vr->Set("ShowSensorBar", bShowSensorBar);
  vr->Set("ShowGameCamera", bShowGameCamera);
  vr->Set("ShowGameFrustum", bShowGameFrustum);
  vr->Set("ShowTrackingCamera", bShowTrackingCamera);
  vr->Set("ShowTrackingVolume", bShowTrackingVolume);
  vr->Set("ShowBaseStation", bShowBaseStation);
  vr->Set("MotionSicknessAlways", bMotionSicknessAlways);
  vr->Set("MotionSicknessFreelook", bMotionSicknessFreelook);
  vr->Set("MotionSickness2D", bMotionSickness2D);
  vr->Set("MotionSicknessLeftStick", bMotionSicknessLeftStick);
  vr->Set("MotionSicknessRightStick", bMotionSicknessRightStick);
  vr->Set("MotionSicknessDPad", bMotionSicknessDPad);
  vr->Set("MotionSicknessIR", bMotionSicknessIR);
  vr->Set("MotionSicknessMethod", iMotionSicknessMethod);
  vr->Set("MotionSicknessSkybox", iMotionSicknessSkybox);
  vr->Set("MotionSicknessFOV", fMotionSicknessFOV);
  vr->Set("Player", iVRPlayer);
  vr->Set("Player2", iVRPlayer2);
  vr->Set("MirrorPlayer", iMirrorPlayer);
  vr->Set("MirrorStyle", iMirrorStyle);
  vr->Set("TimewarpTweak", fTimeWarpTweak);
  vr->Set("NumExtraFrames", iExtraTimewarpedFrames);
  vr->Set("NumExtraVideoLoops", iExtraVideoLoops);
  vr->Set("NumExtraVideoLoopsDivider", iExtraVideoLoopsDivider);
  vr->Set("StabilizeRoll", bStabilizeRoll);
  vr->Set("StabilizePitch", bStabilizePitch);
  vr->Set("StabilizeYaw", bStabilizeYaw);
  vr->Set("StabilizeX", bStabilizeX);
  vr->Set("StabilizeY", bStabilizeY);
  vr->Set("StabilizeZ", bStabilizeZ);
  vr->Set("Keyhole", bKeyhole);
  vr->Set("KeyholeWidth", fKeyholeWidth);
  vr->Set("KeyholeSnap", bKeyholeSnap);
  vr->Set("KeyholeSnapSize", fKeyholeSnapSize);
  vr->Set("PullUp20fps", bPullUp20fps);
  vr->Set("PullUp30fps", bPullUp30fps);
  vr->Set("PullUp60fps", bPullUp60fps);
  vr->Set("PullUpAuto", bPullUpAuto);
  vr->Set("OpcodeReplay", bOpcodeReplay);
  vr->Set("OpcodeWarningDisable", bOpcodeWarningDisable);
  vr->Set("ReplayVertexData", bReplayVertexData);
  vr->Set("ReplayOtherData", bReplayOtherData);
  vr->Set("PullUp20fpsTimewarp", bPullUp20fpsTimewarp);
  vr->Set("PullUp30fpsTimewarp", bPullUp30fpsTimewarp);
  vr->Set("PullUp60fpsTimewarp", bPullUp60fpsTimewarp);
  vr->Set("PullUpAutoTimewarp", bPullUpAutoTimewarp);
  vr->Set("SynchronousTimewarp", bSynchronousTimewarp);
  vr->Set("LeftTexture", sLeftTexture);
  vr->Set("RightTexture", sRightTexture);
  vr->Set("GCLeftTexture", sGCLeftTexture);
  vr->Set("GCRightTexture", sGCRightTexture);

  iniFile.Save(ini_file);
}

bool VideoConfig::IsVSync()
{
  return bVSync && !Core::GetIsThrottlerTempDisabled();
}

bool VideoConfig::VRSettingsModified()
{
  return fUnitsPerMetre != g_SavedConfig.fUnitsPerMetre ||
         fHudThickness != g_SavedConfig.fHudThickness ||
         fHudDistance != g_SavedConfig.fHudDistance || fHud3DCloser != g_SavedConfig.fHud3DCloser ||
         fCameraForward != g_SavedConfig.fCameraForward ||
         fCameraPitch != g_SavedConfig.fCameraPitch || fAimDistance != g_SavedConfig.fAimDistance ||
         fMinFOV != g_SavedConfig.fMinFOV || fScreenHeight != g_SavedConfig.fScreenHeight ||
         fN64FOV != g_SavedConfig.fN64FOV || fScreenThickness != g_SavedConfig.fScreenThickness ||
         fScreenDistance != g_SavedConfig.fScreenDistance ||
         fScreenRight != g_SavedConfig.fScreenRight || fScreenUp != g_SavedConfig.fScreenUp ||
         fScreenPitch != g_SavedConfig.fScreenPitch ||
         fTelescopeMaxFOV != g_SavedConfig.fTelescopeMaxFOV ||
         fReadPitch != g_SavedConfig.fReadPitch || iCameraMinPoly != g_SavedConfig.iCameraMinPoly ||
         bDisable3D != g_SavedConfig.bDisable3D || bHudFullscreen != g_SavedConfig.bHudFullscreen ||
         bHudOnTop != g_SavedConfig.bHudOnTop ||
         fHudDespPosition0 != g_SavedConfig.fHudDespPosition0 ||
         fHudDespPosition1 != g_SavedConfig.fHudDespPosition1 ||
         fHudDespPosition2 != g_SavedConfig.fHudDespPosition2 ||
         bDontClearScreen != g_SavedConfig.bDontClearScreen ||
         bCanReadCameraAngles != g_SavedConfig.bCanReadCameraAngles ||
         bDetectSkybox != g_SavedConfig.bDetectSkybox ||
         iTelescopeEye != g_SavedConfig.iTelescopeEye ||
         iMetroidPrime != g_SavedConfig.iMetroidPrime;
}
