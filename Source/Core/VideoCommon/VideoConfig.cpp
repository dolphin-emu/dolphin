// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/ARBruteForcer.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;
static bool s_has_registered_callback = false;

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
  // Needed for the first frame, I think
  fAspectRatioHackW = 1;
  fAspectRatioHackH = 1;

  // disable all features by default
  backend_info.api_type = APIType::Nothing;
  backend_info.MaxTextureSize = 16384;
  backend_info.bSupportsExclusiveFullscreen = false;
  backend_info.bSupportsMultithreading = false;
  backend_info.bSupportsInternalResolutionFrameDumps = false;
  backend_info.bSupportsST3CTextures = false;
  backend_info.bSupportsBPTCTextures = false;

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

  iSelectedLayer = -2;
  iFlashState = 0;
}

void VideoConfig::Refresh()
{
  INFO_LOG(CORE, "VideoConfig::Refresh();");
  if (!s_has_registered_callback)
  {
    // There was a race condition between the video thread and the host thread here, if
    // corrections need to be made by VerifyValidity(). Briefly, the config will contain
    // invalid values. Instead, pause emulation first, which will flush the video thread,
    // update the config and correct it, then resume emulation, after which the video
    // thread will detect the config has changed and act accordingly.
    Config::AddConfigChangedCallback([]() { Core::RunAsCPUThread([]() { g_Config.Refresh(); }); });
    s_has_registered_callback = true;
  }

  if (ARBruteForcer::ch_bruteforce)
    bVSync = false;
  else
    bVSync = Config::Get(Config::GFX_VSYNC);
  iAdapter = Config::Get(Config::GFX_ADAPTER);

  bWidescreenHack = Config::Get(Config::GFX_WIDESCREEN_HACK);
  const int aspect_ratio = Config::Get(Config::GFX_ASPECT_RATIO);
  if (aspect_ratio == ASPECT_AUTO)
    iAspectRatio = Config::Get(Config::GFX_SUGGESTED_ASPECT_RATIO);
  else
    iAspectRatio = aspect_ratio;
  bCrop = Config::Get(Config::GFX_CROP);
  bUseXFB = Config::Get(Config::GFX_USE_XFB);
  bUseRealXFB = Config::Get(Config::GFX_USE_REAL_XFB);
  iSafeTextureCache_ColorSamples = Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);
  bShowFPS = Config::Get(Config::GFX_SHOW_FPS);
  bShowNetPlayPing = Config::Get(Config::GFX_SHOW_NETPLAY_PING);
  bShowNetPlayMessages = Config::Get(Config::GFX_SHOW_NETPLAY_MESSAGES);
  bLogRenderTimeToFile = Config::Get(Config::GFX_LOG_RENDER_TIME_TO_FILE);
  bOverlayStats = Config::Get(Config::GFX_OVERLAY_STATS);
  bOverlayProjStats = Config::Get(Config::GFX_OVERLAY_PROJ_STATS);
  bDumpTextures = Config::Get(Config::GFX_DUMP_TEXTURES);
  bHiresTextures = Config::Get(Config::GFX_HIRES_TEXTURES);
  bConvertHiresTextures = Config::Get(Config::GFX_CONVERT_HIRES_TEXTURES);
  bCacheHiresTextures = Config::Get(Config::GFX_CACHE_HIRES_TEXTURES);
  bDumpEFBTarget = Config::Get(Config::GFX_DUMP_EFB_TARGET);
  bDumpFramesAsImages = Config::Get(Config::GFX_DUMP_FRAMES_AS_IMAGES);
  bFreeLook = Config::Get(Config::GFX_FREE_LOOK);
  bUseFFV1 = Config::Get(Config::GFX_USE_FFV1);
  sDumpFormat = Config::Get(Config::GFX_DUMP_FORMAT);
  sDumpCodec = Config::Get(Config::GFX_DUMP_CODEC);
  sDumpPath = Config::Get(Config::GFX_DUMP_PATH);
  iBitrateKbps = Config::Get(Config::GFX_BITRATE_KBPS);
  bInternalResolutionFrameDumps = Config::Get(Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS);
  bEnableGPUTextureDecoding = Config::Get(Config::GFX_ENABLE_GPU_TEXTURE_DECODING);
  bEnablePixelLighting = Config::Get(Config::GFX_ENABLE_PIXEL_LIGHTING);
  bFastDepthCalc = Config::Get(Config::GFX_FAST_DEPTH_CALC);

  if (ARBruteForcer::ch_bruteforce)
  {
    iMultisamples = 0;
    bSSAA = false;
    iEFBScale = SCALE_1X;
	iInternalResolution = 1;
  }
  else
  {
    iMultisamples = Config::Get(Config::GFX_MSAA);
    bSSAA = Config::Get(Config::GFX_SSAA);
    iEFBScale = Config::Get(Config::GFX_EFB_SCALE);
	iInternalResolution = Config::Get(Config::GFX_INTERNAL_RESOLUTION);
	if (iInternalResolution > -2)
	{
      iEFBScale = iInternalResolution + (iInternalResolution >= 0) + (iInternalResolution > 1) + (iInternalResolution > 2);
	}
  }

  bTexFmtOverlayEnable = Config::Get(Config::GFX_TEXFMT_OVERLAY_ENABLE);
  bTexFmtOverlayCenter = Config::Get(Config::GFX_TEXFMT_OVERLAY_CENTER);
  bWireFrame = Config::Get(Config::GFX_ENABLE_WIREFRAME);
  bDisableFog = Config::Get(Config::GFX_DISABLE_FOG);
  bBorderlessFullscreen = Config::Get(Config::GFX_BORDERLESS_FULLSCREEN);
  bEnableValidationLayer = Config::Get(Config::GFX_ENABLE_VALIDATION_LAYER);
  bBackendMultithreading = Config::Get(Config::GFX_BACKEND_MULTITHREADING);
  iCommandBufferExecuteInterval = Config::Get(Config::GFX_COMMAND_BUFFER_EXECUTE_INTERVAL);
  bShaderCache = Config::Get(Config::GFX_SHADER_CACHE);
  bBackgroundShaderCompiling = Config::Get(Config::GFX_BACKGROUND_SHADER_COMPILING);
  bDisableSpecializedShaders = Config::Get(Config::GFX_DISABLE_SPECIALIZED_SHADERS);
  bPrecompileUberShaders = Config::Get(Config::GFX_PRECOMPILE_UBER_SHADERS);
  iShaderCompilerThreads = Config::Get(Config::GFX_SHADER_COMPILER_THREADS);
  iShaderPrecompilerThreads = Config::Get(Config::GFX_SHADER_PRECOMPILER_THREADS);

  bZComploc = Config::Get(Config::GFX_SW_ZCOMPLOC);
  bZFreeze = Config::Get(Config::GFX_SW_ZFREEZE);
  bDumpObjects = Config::Get(Config::GFX_SW_DUMP_OBJECTS);
  bDumpTevStages = Config::Get(Config::GFX_SW_DUMP_TEV_STAGES);
  bDumpTevTextureFetches = Config::Get(Config::GFX_SW_DUMP_TEV_TEX_FETCHES);
  drawStart = Config::Get(Config::GFX_SW_DRAW_START);
  drawEnd = Config::Get(Config::GFX_SW_DRAW_END);

  bForceFiltering = Config::Get(Config::GFX_ENHANCE_FORCE_FILTERING);
  iMaxAnisotropy = Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY);
  sPostProcessingShader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  bForceTrueColor = Config::Get(Config::GFX_ENHANCE_FORCE_TRUE_COLOR);
  if ((g_has_rift || g_has_openvr) && backend_info.bSupportsGeometryShaders)
    iStereoMode = STEREO_OCULUS;

  iStereoMode = Config::Get(Config::GFX_STEREO_MODE);
  iStereoDepth = Config::Get(Config::GFX_STEREO_DEPTH);
  iStereoConvergencePercentage = Config::Get(Config::GFX_STEREO_CONVERGENCE_PERCENTAGE);
  bStereoSwapEyes = Config::Get(Config::GFX_STEREO_SWAP_EYES);
  iStereoConvergence = Config::Get(Config::GFX_STEREO_CONVERGENCE);
  bStereoEFBMonoDepth = Config::Get(Config::GFX_STEREO_EFB_MONO_DEPTH);
  iStereoDepthPercentage = Config::Get(Config::GFX_STEREO_DEPTH_PERCENTAGE);

  bEFBAccessEnable = Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
  bBBoxEnable = Config::Get(Config::GFX_HACK_BBOX_ENABLE);
  bBBoxPreferStencilImplementation =
      Config::Get(Config::GFX_HACK_BBOX_PREFER_STENCIL_IMPLEMENTATION);
  bForceProgressive = Config::Get(Config::GFX_HACK_FORCE_PROGRESSIVE);
  bEFBCopyEnable = Config::Get(Config::GFX_HACK_EFB_COPY_ENABLE);
  bEFBCopyClearDisable = Config::Get(Config::GFX_HACK_EFB_COPY_CLEAR_DISABLE);
  bSkipEFBCopyToRam = Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
  bCopyEFBScaled = Config::Get(Config::GFX_HACK_COPY_EFB_ENABLED);
  bEFBEmulateFormatChanges = Config::Get(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES);
  bVertexRounding = Config::Get(Config::GFX_HACK_VERTEX_ROUDING);

  phack.m_enable = Config::Get(Config::GFX_PROJECTION_HACK) == 1;
  phack.m_sznear = Config::Get(Config::GFX_PROJECTION_HACK_SZNEAR) == 1;
  phack.m_szfar = Config::Get(Config::GFX_PROJECTION_HACK_SZFAR) == 1;
  phack.m_znear = Config::Get(Config::GFX_PROJECTION_HACK_ZNEAR);
  phack.m_zfar = Config::Get(Config::GFX_PROJECTION_HACK_ZFAR);
  bPerfQueriesEnable = Config::Get(Config::GFX_PERF_QUERIES_ENABLE);

  bDisable3D = Config::Get(Config::GFX_VR_DISABLE_3D);
  bHudFullscreen = Config::Get(Config::GFX_VR_HUD_FULLSCREEN);
  bHudOnTop = Config::Get(Config::GFX_VR_HUD_ON_TOP);
  bDontClearScreen = Config::Get(Config::GFX_VR_DONT_CLEAR_SCREEN);
  bCanReadCameraAngles = Config::Get(Config::GFX_VR_CAN_READ_CAMERA_ANGLES);
  bDetectSkybox = Config::Get(Config::GFX_VR_DETECT_SKYBOX);
  fUnitsPerMetre = Config::Get(Config::GFX_VR_UNITS_PER_METRE);
  fHudThickness = Config::Get(Config::GFX_VR_HUD_THICKNESS);
  fHudDistance = Config::Get(Config::GFX_VR_HUD_DISTANCE);
  fHud3DCloser = Config::Get(Config::GFX_VR_HUD_3D_CLOSER);
  fCameraForward = Config::Get(Config::GFX_VR_CAMERA_FORWARD);
  fCameraPitch = Config::Get(Config::GFX_VR_CAMERA_PITCH);
  fAimDistance = Config::Get(Config::GFX_VR_AIM_DISTANCE);
  fMinFOV = Config::Get(Config::GFX_VR_MIN_FOV);
  fN64FOV = Config::Get(Config::GFX_VR_N64_FOV);
  fScreenHeight = Config::Get(Config::GFX_VR_SCREEN_HEIGHT);
  fScreenThickness = Config::Get(Config::GFX_VR_SCREEN_THICKNESS);
  fScreenDistance = Config::Get(Config::GFX_VR_SCREEN_DISTANCE);
  fScreenRight = Config::Get(Config::GFX_VR_SCREEN_RIGHT);
  fScreenUp = Config::Get(Config::GFX_VR_SCREEN_UP);
  fScreenPitch = Config::Get(Config::GFX_VR_SCREEN_PITCH);
  iMetroidPrime = Config::Get(Config::GFX_VR_METROID_PRIME);
  iTelescopeEye = Config::Get(Config::GFX_VR_TELESCOPE_EYE);
  fTelescopeMaxFOV = Config::Get(Config::GFX_VR_TELESCOPE_MAX_FOV);
  fReadPitch = Config::Get(Config::GFX_VR_READ_PITCH);
  iCameraMinPoly = Config::Get(Config::GFX_VR_CAMERA_MIN_POLY);
  fHudDespPosition0 = Config::Get(Config::GFX_VR_HUD_DESP_POSITION_0);
  fHudDespPosition1 = Config::Get(Config::GFX_VR_HUD_DESP_POSITION_1);
  fHudDespPosition2 = Config::Get(Config::GFX_VR_HUD_DESP_POSITION_2);

  if (iEFBScale == SCALE_FORCE_INTEGRAL)
  {
    // Round down to multiple of native IR
    switch (Config::GetBase(Config::GFX_EFB_SCALE))
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
      iEFBScale = Config::GetBase(Config::GFX_EFB_SCALE);
      break;
    }
  }

  bool bNoMirrorToWindow;

  fScale = Config::Get(Config::GLOBAL_VR_SCALE);
  fFreeLookSensitivity = Config::Get(Config::GLOBAL_VR_FREE_LOOK_SENSITIVITY);
  fLeanBackAngle = Config::Get(Config::GLOBAL_VR_LEAN_BACK_ANGLE);
  bEnableVR = Config::Get(Config::GLOBAL_VR_ENABLE_VR);
  bLowPersistence = Config::Get(Config::GLOBAL_VR_LOW_PERSISTENCE);
  bDynamicPrediction = Config::Get(Config::GLOBAL_VR_DYNAMIC_PREDICTION);
  bNoMirrorToWindow = Config::Get(Config::GLOBAL_VR_NO_MIRROR_TO_WINDOW);
  bOrientationTracking = Config::Get(Config::GLOBAL_VR_ORIENTATION_TRACKING);
  bMagYawCorrection = Config::Get(Config::GLOBAL_VR_MAG_YAW_CORRECTION);
  bPositionTracking = Config::Get(Config::GLOBAL_VR_POSITION_TRACKING);
  bChromatic = Config::Get(Config::GLOBAL_VR_CHROMATIC);
  bTimewarp = Config::Get(Config::GLOBAL_VR_TIMEWARP);
  bVignette = Config::Get(Config::GLOBAL_VR_VIGNETTE);
  bNoRestore = Config::Get(Config::GLOBAL_VR_NO_RESTORE);
  bFlipVertical = Config::Get(Config::GLOBAL_VR_FLIP_VERTICAL);
  bSRGB = Config::Get(Config::GLOBAL_VR_SRGB);
  bOverdrive = Config::Get(Config::GLOBAL_VR_OVERDRIVE);
  bHqDistortion = Config::Get(Config::GLOBAL_VR_HQ_DISTORTION);
  bDisableNearClipping = Config::Get(Config::GLOBAL_VR_DISABLE_NEAR_CLIPPING);
  bAutoPairViveControllers = Config::Get(Config::GLOBAL_VR_AUTO_PAIR_VIVE_CONTROLLERS);
  bShowHands = Config::Get(Config::GLOBAL_VR_SHOW_HANDS);
  bShowFeet = Config::Get(Config::GLOBAL_VR_SHOW_FEET);
  bShowController = Config::Get(Config::GLOBAL_VR_SHOW_CONTROLLER);
  bShowLaserPointer = Config::Get(Config::GLOBAL_VR_SHOW_LASER_POINTER);
  bShowAimRectangle = Config::Get(Config::GLOBAL_VR_SHOW_AIM_RECTANGLE);
  bShowHudBox = Config::Get(Config::GLOBAL_VR_SHOW_HUD_BOX);
  bShow2DBox = Config::Get(Config::GLOBAL_VR_SHOW_2D_SCREEN_BOX);
  bShowSensorBar = Config::Get(Config::GLOBAL_VR_SHOW_SENSOR_BAR);
  bShowGameCamera = Config::Get(Config::GLOBAL_VR_SHOW_GAME_CAMERA);
  bShowGameFrustum = Config::Get(Config::GLOBAL_VR_SHOW_GAME_FRUSTUM);
  bShowTrackingCamera = Config::Get(Config::GLOBAL_VR_SHOW_TRACKING_CAMERA);
  bShowTrackingVolume = Config::Get(Config::GLOBAL_VR_SHOW_TRACKING_VOLUME);
  bShowBaseStation = Config::Get(Config::GLOBAL_VR_SHOW_BASE_STATION);
  bMotionSicknessAlways = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_ALWAYS);
  bMotionSicknessFreelook = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_FREELOOK);
  bMotionSickness2D = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_2D);
  bMotionSicknessLeftStick = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_LEFT_STICK);
  bMotionSicknessRightStick = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_RIGHT_STICK);
  bMotionSicknessDPad = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_DPAD);
  bMotionSicknessIR = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_IR);
  iMotionSicknessMethod = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_METHOD);
  iMotionSicknessSkybox = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_SKYBOX);
  fMotionSicknessFOV = Config::Get(Config::GLOBAL_VR_MOTION_SICKNESS_FOV);
  iVRPlayer = Config::Get(Config::GLOBAL_VR_PLAYER);
  iVRPlayer2 = Config::Get(Config::GLOBAL_VR_PLAYER_2);
  iMirrorPlayer = Config::Get(Config::GLOBAL_VR_MIRROR_PLAYER);

  // TODO: This doesn't work anymore, because we want to specify a default for a setting based on
  // another existing setting
  iMirrorStyle = bNoMirrorToWindow ? VR_MIRROR_DISABLED : VR_MIRROR_LEFT;
  iMirrorStyle = Config::Get(Config::GLOBAL_VR_MIRROR_STYLE);

  fTimeWarpTweak = Config::Get(Config::GLOBAL_VR_TIMEWARP_TWEAK);
  iExtraTimewarpedFrames = Config::Get(Config::GLOBAL_VR_NUM_EXTRA_FRAMES);
  iExtraVideoLoops = Config::Get(Config::GLOBAL_VR_NUM_EXTRA_VIDEO_LOOPS);
  iExtraVideoLoopsDivider = Config::Get(Config::GLOBAL_VR_NUM_EXTRA_VIDEO_LOOPS_DIVIDER);
  bStabilizeRoll = Config::Get(Config::GLOBAL_VR_STABILIZE_ROLL);
  bStabilizePitch = Config::Get(Config::GLOBAL_VR_STABILIZE_PITCH);
  bStabilizeYaw = Config::Get(Config::GLOBAL_VR_STABILIZE_YAW);
  bStabilizeX = Config::Get(Config::GLOBAL_VR_STABILIZE_X);
  bStabilizeY = Config::Get(Config::GLOBAL_VR_STABILIZE_Y);
  bStabilizeZ = Config::Get(Config::GLOBAL_VR_STABILIZE_Z);
  bKeyhole = Config::Get(Config::GLOBAL_VR_KEYHOLE);
  fKeyholeWidth = Config::Get(Config::GLOBAL_VR_KEYHOLE_WIDTH);
  bKeyholeSnap = Config::Get(Config::GLOBAL_VR_KEYHOLE_SNAP);
  fKeyholeSnapSize = Config::Get(Config::GLOBAL_VR_KEYHOLE_SIZE);
  bPullUp20fps = Config::Get(Config::GLOBAL_VR_PULL_UP_20_FPS);
  bPullUp30fps = Config::Get(Config::GLOBAL_VR_PULL_UP_30_FPS);
  bPullUp60fps = Config::Get(Config::GLOBAL_VR_PULL_UP_60_FPS);
  bPullUpAuto = Config::Get(Config::GLOBAL_VR_PULL_UP_AUTO);
  bOpcodeReplay = Config::Get(Config::GLOBAL_VR_OPCODE_REPLAY);
  bOpcodeWarningDisable = Config::Get(Config::GLOBAL_VR_OPCODE_WARNING_DISABLE);
  bReplayVertexData = Config::Get(Config::GLOBAL_VR_REPLAY_VERTEX_DATA);
  bReplayOtherData = Config::Get(Config::GLOBAL_VR_REPLAY_OTHER_DATA);
  bPullUp20fpsTimewarp = Config::Get(Config::GLOBAL_VR_PULL_UP_20_FPS_TIMEWARP);
  bPullUp30fpsTimewarp = Config::Get(Config::GLOBAL_VR_PULL_UP_30_FPS_TIMEWARP);
  bPullUp60fpsTimewarp = Config::Get(Config::GLOBAL_VR_PULL_UP_60_FPS_TIMEWARP);
  bPullUpAutoTimewarp = Config::Get(Config::GLOBAL_VR_PULL_UP_AUTO_TIMEWARP);
  bSynchronousTimewarp = Config::Get(Config::GLOBAL_VR_SYNCHRONOUS_TIMEWARP);
  sLeftTexture = Config::Get(Config::GLOBAL_VR_LEFT_TEXTURE);
  sRightTexture = Config::Get(Config::GLOBAL_VR_RIGHT_TEXTURE);
  sGCLeftTexture = Config::Get(Config::GLOBAL_VR_GC_LEFT_TEXTURE);
  sGCRightTexture = Config::Get(Config::GLOBAL_VR_GC_RIGHT_TEXTURE);

  // LoadVR(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");

  VerifyValidity();
}

void VideoConfig::GameIniSave()
{
  // Save game ini
  INFO_LOG(CORE, "VideoConfig::GameIniSave()");

  if (!Config::LayerExists(Config::LayerType::LocalGame))
    return;

  Config::SaveIfNotDefault(Config::GFX_VR_DISABLE_3D);

  Config::SaveIfNotDefault(Config::GFX_VR_UNITS_PER_METRE);
  Config::SaveIfNotDefault(Config::GFX_VR_HUD_FULLSCREEN);
  Config::SaveIfNotDefault(Config::GFX_VR_HUD_ON_TOP);
  Config::SaveIfNotDefault(Config::GFX_VR_DONT_CLEAR_SCREEN);
  Config::SaveIfNotDefault(Config::GFX_VR_CAN_READ_CAMERA_ANGLES);
  Config::SaveIfNotDefault(Config::GFX_VR_DETECT_SKYBOX);
  Config::SaveIfNotDefault(Config::GFX_VR_HUD_DISTANCE);
  Config::SaveIfNotDefault(Config::GFX_VR_HUD_THICKNESS);
  Config::SaveIfNotDefault(Config::GFX_VR_HUD_3D_CLOSER);
  Config::SaveIfNotDefault(Config::GFX_VR_CAMERA_FORWARD);
  Config::SaveIfNotDefault(Config::GFX_VR_CAMERA_PITCH);
  Config::SaveIfNotDefault(Config::GFX_VR_AIM_DISTANCE);
  Config::SaveIfNotDefault(Config::GFX_VR_MIN_FOV);
  Config::SaveIfNotDefault(Config::GFX_VR_N64_FOV);
  Config::SaveIfNotDefault(Config::GFX_VR_SCREEN_HEIGHT);
  Config::SaveIfNotDefault(Config::GFX_VR_SCREEN_DISTANCE);
  Config::SaveIfNotDefault(Config::GFX_VR_SCREEN_THICKNESS);
  Config::SaveIfNotDefault(Config::GFX_VR_SCREEN_UP);
  Config::SaveIfNotDefault(Config::GFX_VR_SCREEN_RIGHT);
  Config::SaveIfNotDefault(Config::GFX_VR_SCREEN_PITCH);
  Config::SaveIfNotDefault(Config::GFX_VR_READ_PITCH);
  Config::SaveIfNotDefault(Config::GFX_VR_HUD_DESP_POSITION_0);
  Config::SaveIfNotDefault(Config::GFX_VR_HUD_DESP_POSITION_1);
  Config::SaveIfNotDefault(Config::GFX_VR_HUD_DESP_POSITION_2);
  Config::SaveIfNotDefault(Config::GFX_VR_CAMERA_MIN_POLY);
  Config::Layer* local = Config::GetLayer(Config::LayerType::LocalGame);
  if (local)
    local->Save();
  Refresh();
  g_SavedConfig = *this;
}

void VideoConfig::GameIniReset()
{
  Config::ResetToGameDefault(Config::GFX_VR_DISABLE_3D);
  Config::ResetToGameDefault(Config::GFX_VR_UNITS_PER_METRE);
  Config::ResetToGameDefault(Config::GFX_VR_HUD_FULLSCREEN);
  Config::ResetToGameDefault(Config::GFX_VR_HUD_3D_CLOSER);
  Config::ResetToGameDefault(Config::GFX_VR_HUD_DISTANCE);
  Config::ResetToGameDefault(Config::GFX_VR_HUD_THICKNESS);
  Config::ResetToGameDefault(Config::GFX_VR_CAMERA_FORWARD);
  Config::ResetToGameDefault(Config::GFX_VR_CAMERA_PITCH);
  Config::ResetToGameDefault(Config::GFX_VR_AIM_DISTANCE);
  Config::ResetToGameDefault(Config::GFX_VR_SCREEN_HEIGHT);
  Config::ResetToGameDefault(Config::GFX_VR_SCREEN_DISTANCE);
  Config::ResetToGameDefault(Config::GFX_VR_SCREEN_THICKNESS);
  Config::ResetToGameDefault(Config::GFX_VR_SCREEN_UP);
  Config::ResetToGameDefault(Config::GFX_VR_SCREEN_RIGHT);
  Config::ResetToGameDefault(Config::GFX_VR_SCREEN_PITCH);
  Config::ResetToGameDefault(Config::GFX_VR_MIN_FOV);
  Config::ResetToGameDefault(Config::GFX_VR_N64_FOV);
  Config::ResetToGameDefault(Config::GFX_VR_READ_PITCH);
  Config::ResetToGameDefault(Config::GFX_VR_CAMERA_MIN_POLY);
  Config::ResetToGameDefault(Config::GFX_VR_HUD_DESP_POSITION_0);
  Config::ResetToGameDefault(Config::GFX_VR_HUD_DESP_POSITION_1);
  Config::ResetToGameDefault(Config::GFX_VR_HUD_DESP_POSITION_2);
  Config::ResetToGameDefault(Config::GFX_VR_CAN_READ_CAMERA_ANGLES);
  Config::ResetToGameDefault(Config::GFX_VR_DETECT_SKYBOX);
  Config::ResetToGameDefault(Config::GFX_VR_HUD_ON_TOP);
  Config::ResetToGameDefault(Config::GFX_VR_DONT_CLEAR_SCREEN);
  Config::Save();
  g_Config.Refresh();
}

void VideoConfig::VerifyValidity()
{
  INFO_LOG(CORE, "VideoConfig::VerifyValidity()");
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

bool VideoConfig::IsVSync() const
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

static u32 GetNumAutoShaderCompilerThreads()
{
  // Automatic number. We use clamp(cpus - 3, 1, 4).
  return static_cast<u32>(std::min(std::max(cpu_info.num_cores - 3, 1), 4));
}

u32 VideoConfig::GetShaderCompilerThreads() const
{
  if (iShaderCompilerThreads >= 0)
    return static_cast<u32>(iShaderCompilerThreads);
  else
    return GetNumAutoShaderCompilerThreads();
}

u32 VideoConfig::GetShaderPrecompilerThreads() const
{
  if (iShaderPrecompilerThreads >= 0)
    return static_cast<u32>(iShaderPrecompilerThreads);
  else
    return GetNumAutoShaderCompilerThreads();
}

bool VideoConfig::CanPrecompileUberShaders() const
{
  // We don't want to precompile ubershaders if they're never going to be used.
  return bPrecompileUberShaders && (bBackgroundShaderCompiling || bDisableSpecializedShaders);
}

bool VideoConfig::CanBackgroundCompileShaders() const
{
  // We require precompiled ubershaders to background compile shaders.
  return bBackgroundShaderCompiling && bPrecompileUberShaders;
}
