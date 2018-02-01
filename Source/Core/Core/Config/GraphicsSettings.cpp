// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/GraphicsSettings.h"

#include <string>

#include "Common/Config/Config.h"
#include "Common/StringUtil.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

namespace Config
{
// Configuration Information

// Graphics.Hardware

const ConfigInfo<bool> GFX_VSYNC{{System::GFX, "Hardware", "VSync"}, false};
const ConfigInfo<int> GFX_ADAPTER{{System::GFX, "Hardware", "Adapter"}, 0};

// Graphics.Settings

const ConfigInfo<bool> GFX_WIDESCREEN_HACK{{System::GFX, "Settings", "wideScreenHack"}, false};
const ConfigInfo<int> GFX_ASPECT_RATIO{{System::GFX, "Settings", "AspectRatio"},
                                       static_cast<int>(ASPECT_AUTO)};
const ConfigInfo<int> GFX_SUGGESTED_ASPECT_RATIO{{System::GFX, "Settings", "SuggestedAspectRatio"},
                                                 static_cast<int>(ASPECT_AUTO)};
const ConfigInfo<bool> GFX_CROP{{System::GFX, "Settings", "Crop"}, false};
const ConfigInfo<bool> GFX_USE_XFB{{System::GFX, "Settings", "UseXFB"}, false};
const ConfigInfo<bool> GFX_USE_REAL_XFB{{System::GFX, "Settings", "UseRealXFB"}, false};
const ConfigInfo<int> GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES{
    {System::GFX, "Settings", "SafeTextureCacheColorSamples"}, 128};
const ConfigInfo<bool> GFX_SHOW_FPS{{System::GFX, "Settings", "ShowFPS"}, false};
const ConfigInfo<bool> GFX_SHOW_NETPLAY_PING{{System::GFX, "Settings", "ShowNetPlayPing"}, false};
const ConfigInfo<bool> GFX_SHOW_NETPLAY_MESSAGES{{System::GFX, "Settings", "ShowNetPlayMessages"},
                                                 false};
const ConfigInfo<bool> GFX_LOG_RENDER_TIME_TO_FILE{{System::GFX, "Settings", "LogRenderTimeToFile"},
                                                   false};
const ConfigInfo<bool> GFX_OVERLAY_STATS{{System::GFX, "Settings", "OverlayStats"}, false};
const ConfigInfo<bool> GFX_OVERLAY_PROJ_STATS{{System::GFX, "Settings", "OverlayProjStats"}, false};
const ConfigInfo<bool> GFX_DUMP_TEXTURES{{System::GFX, "Settings", "DumpTextures"}, false};
const ConfigInfo<bool> GFX_HIRES_TEXTURES{{System::GFX, "Settings", "HiresTextures"}, false};
const ConfigInfo<bool> GFX_CONVERT_HIRES_TEXTURES{{System::GFX, "Settings", "ConvertHiresTextures"},
                                                  false};
const ConfigInfo<bool> GFX_CACHE_HIRES_TEXTURES{{System::GFX, "Settings", "CacheHiresTextures"},
                                                false};
const ConfigInfo<bool> GFX_DUMP_EFB_TARGET{{System::GFX, "Settings", "DumpEFBTarget"}, false};
const ConfigInfo<bool> GFX_DUMP_FRAMES_AS_IMAGES{{System::GFX, "Settings", "DumpFramesAsImages"},
                                                 false};
const ConfigInfo<bool> GFX_FREE_LOOK{{System::GFX, "Settings", "FreeLook"}, false};
const ConfigInfo<bool> GFX_USE_FFV1{{System::GFX, "Settings", "UseFFV1"}, false};
const ConfigInfo<std::string> GFX_DUMP_FORMAT{{System::GFX, "Settings", "DumpFormat"}, "avi"};
const ConfigInfo<std::string> GFX_DUMP_CODEC{{System::GFX, "Settings", "DumpCodec"}, ""};
const ConfigInfo<std::string> GFX_DUMP_PATH{{System::GFX, "Settings", "DumpPath"}, ""};
const ConfigInfo<int> GFX_BITRATE_KBPS{{System::GFX, "Settings", "BitrateKbps"}, 2500};
const ConfigInfo<bool> GFX_INTERNAL_RESOLUTION_FRAME_DUMPS{
    {System::GFX, "Settings", "InternalResolutionFrameDumps"}, false};
const ConfigInfo<bool> GFX_ENABLE_GPU_TEXTURE_DECODING{
    {System::GFX, "Settings", "EnableGPUTextureDecoding"}, false};
const ConfigInfo<bool> GFX_ENABLE_PIXEL_LIGHTING{{System::GFX, "Settings", "EnablePixelLighting"},
                                                 false};
const ConfigInfo<bool> GFX_FAST_DEPTH_CALC{{System::GFX, "Settings", "FastDepthCalc"}, true};
const ConfigInfo<u32> GFX_MSAA{{System::GFX, "Settings", "MSAA"}, 1};
const ConfigInfo<bool> GFX_SSAA{{System::GFX, "Settings", "SSAA"}, false};
const ConfigInfo<int> GFX_EFB_SCALE{{System::GFX, "Settings", "EFBScale"},
                                    static_cast<int>(SCALE_1X)};
const ConfigInfo<int> GFX_INTERNAL_RESOLUTION{{System::GFX, "Settings", "InternalResolution"}, -2};
const ConfigInfo<bool> GFX_TEXFMT_OVERLAY_ENABLE{{System::GFX, "Settings", "TexFmtOverlayEnable"},
                                                 false};
const ConfigInfo<bool> GFX_TEXFMT_OVERLAY_CENTER{{System::GFX, "Settings", "TexFmtOverlayCenter"},
                                                 false};
const ConfigInfo<bool> GFX_ENABLE_WIREFRAME{{System::GFX, "Settings", "WireFrame"}, false};
const ConfigInfo<bool> GFX_DISABLE_FOG{{System::GFX, "Settings", "DisableFog"}, false};
const ConfigInfo<bool> GFX_BORDERLESS_FULLSCREEN{{System::GFX, "Settings", "BorderlessFullscreen"},
                                                 false};
const ConfigInfo<bool> GFX_ENABLE_VALIDATION_LAYER{
    {System::GFX, "Settings", "EnableValidationLayer"}, false};
const ConfigInfo<bool> GFX_BACKEND_MULTITHREADING{
    {System::GFX, "Settings", "BackendMultithreading"}, true};
const ConfigInfo<int> GFX_COMMAND_BUFFER_EXECUTE_INTERVAL{
    {System::GFX, "Settings", "CommandBufferExecuteInterval"}, 100};
const ConfigInfo<bool> GFX_SHADER_CACHE{{System::GFX, "Settings", "ShaderCache"}, true};
const ConfigInfo<bool> GFX_BACKGROUND_SHADER_COMPILING{
    {System::GFX, "Settings", "BackgroundShaderCompiling"}, false};
const ConfigInfo<bool> GFX_DISABLE_SPECIALIZED_SHADERS{
    {System::GFX, "Settings", "DisableSpecializedShaders"}, false};
const ConfigInfo<bool> GFX_PRECOMPILE_UBER_SHADERS{
    {System::GFX, "Settings", "PrecompileUberShaders"}, true};
const ConfigInfo<int> GFX_SHADER_COMPILER_THREADS{
    {System::GFX, "Settings", "ShaderCompilerThreads"}, 1};
const ConfigInfo<int> GFX_SHADER_PRECOMPILER_THREADS{
    {System::GFX, "Settings", "ShaderPrecompilerThreads"}, 1};

const ConfigInfo<bool> GFX_SW_ZCOMPLOC{{System::GFX, "Settings", "SWZComploc"}, true};
const ConfigInfo<bool> GFX_SW_ZFREEZE{{System::GFX, "Settings", "SWZFreeze"}, true};
const ConfigInfo<bool> GFX_SW_DUMP_OBJECTS{{System::GFX, "Settings", "SWDumpObjects"}, false};
const ConfigInfo<bool> GFX_SW_DUMP_TEV_STAGES{{System::GFX, "Settings", "SWDumpTevStages"}, false};
const ConfigInfo<bool> GFX_SW_DUMP_TEV_TEX_FETCHES{{System::GFX, "Settings", "SWDumpTevTexFetches"},
                                                   false};
const ConfigInfo<int> GFX_SW_DRAW_START{{System::GFX, "Settings", "SWDrawStart"}, 0};
const ConfigInfo<int> GFX_SW_DRAW_END{{System::GFX, "Settings", "SWDrawEnd"}, 100000};

const ConfigInfo<bool> GFX_PREFER_GLES{{System::GFX, "Settings", "PreferGLES"}, false};

// Graphics.Enhancements

const ConfigInfo<bool> GFX_ENHANCE_FORCE_FILTERING{{System::GFX, "Enhancements", "ForceFiltering"},
                                                   false};
const ConfigInfo<int> GFX_ENHANCE_MAX_ANISOTROPY{{System::GFX, "Enhancements", "MaxAnisotropy"}, 0};
const ConfigInfo<std::string> GFX_ENHANCE_POST_SHADER{
    {System::GFX, "Enhancements", "PostProcessingShader"}, ""};
const ConfigInfo<bool> GFX_ENHANCE_FORCE_TRUE_COLOR{{System::GFX, "Enhancements", "ForceTrueColor"},
                                                    true};

// Graphics.Stereoscopy

const ConfigInfo<int> GFX_STEREO_MODE{{System::GFX, "Stereoscopy", "StereoMode"}, 0};
const ConfigInfo<int> GFX_STEREO_DEPTH{{System::GFX, "Stereoscopy", "StereoDepth"}, 20};
const ConfigInfo<int> GFX_STEREO_CONVERGENCE_PERCENTAGE{
    {System::GFX, "Stereoscopy", "StereoConvergencePercentage"}, 100};
const ConfigInfo<bool> GFX_STEREO_SWAP_EYES{{System::GFX, "Stereoscopy", "StereoSwapEyes"}, false};
const ConfigInfo<int> GFX_STEREO_CONVERGENCE{{System::GFX, "Stereoscopy", "StereoConvergence"}, 20};
const ConfigInfo<bool> GFX_STEREO_EFB_MONO_DEPTH{{System::GFX, "Stereoscopy", "StereoEFBMonoDepth"},
                                                 false};
const ConfigInfo<int> GFX_STEREO_DEPTH_PERCENTAGE{
    {System::GFX, "Stereoscopy", "StereoDepthPercentage"}, 100};

// Graphics.Hacks

const ConfigInfo<bool> GFX_HACK_EFB_ACCESS_ENABLE{{System::GFX, "Hacks", "EFBAccessEnable"}, true};
const ConfigInfo<bool> GFX_HACK_BBOX_ENABLE{{System::GFX, "Hacks", "BBoxEnable"}, false};
const ConfigInfo<bool> GFX_HACK_BBOX_PREFER_STENCIL_IMPLEMENTATION{
    {System::GFX, "Hacks", "BBoxPreferStencilImplementation"}, false};
const ConfigInfo<bool> GFX_HACK_FORCE_PROGRESSIVE{{System::GFX, "Hacks", "ForceProgressive"}, true};
const ConfigInfo<bool> GFX_HACK_EFB_COPY_ENABLE{{System::GFX, "Hacks", "EFBCopyEnable"}, true};
const ConfigInfo<bool> GFX_HACK_EFB_COPY_CLEAR_DISABLE{
    {System::GFX, "Hacks", "EFBCopyClearDisable"}, false};
const ConfigInfo<bool> GFX_HACK_SKIP_EFB_COPY_TO_RAM{{System::GFX, "Hacks", "EFBToTextureEnable"},
                                                     true};
const ConfigInfo<bool> GFX_HACK_COPY_EFB_ENABLED{{System::GFX, "Hacks", "EFBScaledCopy"}, true};
const ConfigInfo<bool> GFX_HACK_EFB_EMULATE_FORMAT_CHANGES{
    {System::GFX, "Hacks", "EFBEmulateFormatChanges"}, false};
const ConfigInfo<bool> GFX_HACK_VERTEX_ROUDING{{System::GFX, "Hacks", "VertexRounding"}, false};

// Graphics.GameSpecific

const ConfigInfo<int> GFX_PROJECTION_HACK{{System::GFX, "GameSpecific", "ProjectionHack"}, 0};
const ConfigInfo<int> GFX_PROJECTION_HACK_SZNEAR{{System::GFX, "GameSpecific", "PH_SZNear"}, 0};
const ConfigInfo<int> GFX_PROJECTION_HACK_SZFAR{{System::GFX, "GameSpecific", "PH_SZFar"}, 0};
const ConfigInfo<std::string> GFX_PROJECTION_HACK_ZNEAR{{System::GFX, "GameSpecific", "PH_ZNear"},
                                                        ""};
const ConfigInfo<std::string> GFX_PROJECTION_HACK_ZFAR{{System::GFX, "GameSpecific", "PH_ZFar"},
                                                       ""};
const ConfigInfo<bool> GFX_PERF_QUERIES_ENABLE{{System::GFX, "GameSpecific", "PerfQueriesEnable"},
                                               false};
// Graphics.Game Specific VR Settings

const ConfigInfo<bool> GFX_VR_DISABLE_3D{{System::GFX, "VR", "Disable3D"}, false};
const ConfigInfo<bool> GFX_VR_HUD_FULLSCREEN{{System::GFX, "VR", "HudFullscreen"}, false};
const ConfigInfo<bool> GFX_VR_HUD_ON_TOP{{System::GFX, "VR", "HudOnTop"}, false};
const ConfigInfo<bool> GFX_VR_DONT_CLEAR_SCREEN{{System::GFX, "VR", "DontClearScreen"}, false};
const ConfigInfo<bool> GFX_VR_CAN_READ_CAMERA_ANGLES{{System::GFX, "VR", "CanReadCameraAngles"},
                                                     false};
const ConfigInfo<bool> GFX_VR_DETECT_SKYBOX{{System::GFX, "VR", "DetectSkybox"}, false};
const ConfigInfo<float> GFX_VR_UNITS_PER_METRE{{System::GFX, "VR", "UnitsPerMetre"},
                                               DEFAULT_VR_UNITS_PER_METRE};
const ConfigInfo<float> GFX_VR_HUD_THICKNESS{{System::GFX, "VR", "HudThickness"},
                                             DEFAULT_VR_HUD_THICKNESS};
const ConfigInfo<float> GFX_VR_HUD_DISTANCE{{System::GFX, "VR", "HudDistance"},
                                            DEFAULT_VR_HUD_DISTANCE};
const ConfigInfo<float> GFX_VR_HUD_3D_CLOSER{{System::GFX, "VR", "Hud3DCloser"},
                                             DEFAULT_VR_HUD_3D_CLOSER};
const ConfigInfo<float> GFX_VR_CAMERA_FORWARD{{System::GFX, "VR", "CameraForward"},
                                              DEFAULT_VR_CAMERA_FORWARD};
const ConfigInfo<float> GFX_VR_CAMERA_PITCH{{System::GFX, "VR", "CameraPitch"},
                                            DEFAULT_VR_CAMERA_PITCH};
const ConfigInfo<float> GFX_VR_AIM_DISTANCE{{System::GFX, "VR", "AimDistance"},
                                            DEFAULT_VR_AIM_DISTANCE};
const ConfigInfo<float> GFX_VR_MIN_FOV{{System::GFX, "VR", "MinFOV"}, DEFAULT_VR_MIN_FOV};
const ConfigInfo<float> GFX_VR_N64_FOV{{System::GFX, "VR", "N64FOV"}, DEFAULT_VR_N64_FOV};
const ConfigInfo<float> GFX_VR_SCREEN_HEIGHT{{System::GFX, "VR", "ScreenHeight"},
                                             DEFAULT_VR_SCREEN_HEIGHT};
const ConfigInfo<float> GFX_VR_SCREEN_THICKNESS{{System::GFX, "VR", "ScreenThickness"},
                                                DEFAULT_VR_HUD_THICKNESS};
const ConfigInfo<float> GFX_VR_SCREEN_DISTANCE{{System::GFX, "VR", "ScreenDistance"},
                                               DEFAULT_VR_SCREEN_DISTANCE};
const ConfigInfo<float> GFX_VR_SCREEN_RIGHT{{System::GFX, "VR", "ScreenRight"},
                                            DEFAULT_VR_SCREEN_RIGHT};
const ConfigInfo<float> GFX_VR_SCREEN_UP{{System::GFX, "VR", "ScreenUp"}, DEFAULT_VR_SCREEN_UP};
const ConfigInfo<float> GFX_VR_SCREEN_PITCH{{System::GFX, "VR", "ScreenPitch"},
                                            DEFAULT_VR_SCREEN_PITCH};
const ConfigInfo<int> GFX_VR_METROID_PRIME{{System::GFX, "VR", "MetroidPrime"}, 0};
const ConfigInfo<int> GFX_VR_TELESCOPE_EYE{{System::GFX, "VR", "TelescopeEye"}, 0};
const ConfigInfo<float> GFX_VR_TELESCOPE_MAX_FOV{{System::GFX, "VR", "TelescopeFOV"}, 0.0f};
const ConfigInfo<float> GFX_VR_READ_PITCH{{System::GFX, "VR", "ReadPitch"}, 0.0f};
const ConfigInfo<u32> GFX_VR_CAMERA_MIN_POLY{{System::GFX, "VR", "CameraMinPoly"}, 0};
const ConfigInfo<float> GFX_VR_HUD_DESP_POSITION_0{{System::GFX, "VR", "HudDespPosition0"}, 0.0f};
const ConfigInfo<float> GFX_VR_HUD_DESP_POSITION_1{{System::GFX, "VR", "HudDespPosition1"}, 0.0f};
const ConfigInfo<float> GFX_VR_HUD_DESP_POSITION_2{{System::GFX, "VR", "HudDespPosition2"}, 0.0f};

// Global VR Settings

const ConfigInfo<float> GLOBAL_VR_SCALE{{System::Main, "VR", "Scale"}, 1.0f};
const ConfigInfo<float> GLOBAL_VR_FREE_LOOK_SENSITIVITY{{System::Main, "VR", "FreeLookSensitivity"},
                                                        DEFAULT_VR_FREE_LOOK_SENSITIVITY};
const ConfigInfo<float> GLOBAL_VR_LEAN_BACK_ANGLE{{System::Main, "VR", "LeanBackAngle"}, 0.0f};
const ConfigInfo<bool> GLOBAL_VR_ENABLE_VR{{System::Main, "VR", "EnableVR"}, true};
const ConfigInfo<bool> GLOBAL_VR_LOW_PERSISTENCE{{System::Main, "VR", "LowPersistence"}, true};
const ConfigInfo<bool> GLOBAL_VR_DYNAMIC_PREDICTION{{System::Main, "VR", "DynamicPrediction"},
                                                    true};
const ConfigInfo<bool> GLOBAL_VR_NO_MIRROR_TO_WINDOW{{System::Main, "VR", "NoMirrorToWindow"},
                                                     false};
const ConfigInfo<bool> GLOBAL_VR_ORIENTATION_TRACKING{{System::Main, "VR", "OrientationTracking"},
                                                      true};
const ConfigInfo<bool> GLOBAL_VR_MAG_YAW_CORRECTION{{System::Main, "VR", "MagYawCorrection"}, true};
const ConfigInfo<bool> GLOBAL_VR_POSITION_TRACKING{{System::Main, "VR", "PositionTracking"}, true};
const ConfigInfo<bool> GLOBAL_VR_CHROMATIC{{System::Main, "VR", "Chromatic"}, true};
const ConfigInfo<bool> GLOBAL_VR_TIMEWARP{{System::Main, "VR", "Timewarp"}, true};
const ConfigInfo<bool> GLOBAL_VR_VIGNETTE{{System::Main, "VR", "Vignette"}, false};
const ConfigInfo<bool> GLOBAL_VR_NO_RESTORE{{System::Main, "VR", "NoRestore"}, false};
const ConfigInfo<bool> GLOBAL_VR_FLIP_VERTICAL{{System::Main, "VR", "FlipVertical"}, false};
const ConfigInfo<bool> GLOBAL_VR_SRGB{{System::Main, "VR", "sRGB"}, false};
const ConfigInfo<bool> GLOBAL_VR_OVERDRIVE{{System::Main, "VR", "Overdrive"}, true};
const ConfigInfo<bool> GLOBAL_VR_HQ_DISTORTION{{System::Main, "VR", "HQDistortion"}, false};
const ConfigInfo<bool> GLOBAL_VR_DISABLE_NEAR_CLIPPING{{System::Main, "VR", "DisableNearClipping"},
                                                       true};
const ConfigInfo<bool> GLOBAL_VR_AUTO_PAIR_VIVE_CONTROLLERS{
    {System::Main, "VR", "AutoPairViveControllers"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_HANDS{{System::Main, "VR", "ShowHands"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_FEET{{System::Main, "VR", "ShowFeet"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_CONTROLLER{{System::Main, "VR", "ShowController"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_LASER_POINTER{{System::Main, "VR", "ShowLaserPointer"},
                                                    false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_AIM_RECTANGLE{{System::Main, "VR", "ShowAimRectangle"},
                                                    false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_HUD_BOX{{System::Main, "VR", "ShowHudBox"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_2D_SCREEN_BOX{{System::Main, "VR", "Show2DScreenBox"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_SENSOR_BAR{{System::Main, "VR", "ShowSensorBar"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_GAME_CAMERA{{System::Main, "VR", "ShowGameCamera"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_GAME_FRUSTUM{{System::Main, "VR", "ShowGameFrustum"}, false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_TRACKING_CAMERA{{System::Main, "VR", "ShowTrackingCamera"},
                                                      false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_TRACKING_VOLUME{{System::Main, "VR", "ShowTrackingVolume"},
                                                      false};
const ConfigInfo<bool> GLOBAL_VR_SHOW_BASE_STATION{{System::Main, "VR", "ShowBaseStation"}, false};
const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_ALWAYS{
    {System::Main, "VR", "MotionSicknessAlways"}, false};
const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_FREELOOK{
    {System::Main, "VR", "MotionSicknessFreelook"}, false};
const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_2D{{System::Main, "VR", "MotionSickness2D"},
                                                    false};
const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_LEFT_STICK{
    {System::Main, "VR", "MotionSicknessLeftStick"}, false};
const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_RIGHT_STICK{
    {System::Main, "VR", "MotionSicknessRightStick"}, false};
const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_DPAD{{System::Main, "VR", "MotionSicknessDPad"},
                                                      false};
const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_IR{{System::Main, "VR", "MotionSicknessIR"},
                                                    false};
const ConfigInfo<int> GLOBAL_VR_MOTION_SICKNESS_METHOD{{System::Main, "VR", "MotionSicknessMethod"},
                                                       0};
const ConfigInfo<int> GLOBAL_VR_MOTION_SICKNESS_SKYBOX{{System::Main, "VR", "MotionSicknessSkybox"},
                                                       0};
const ConfigInfo<float> GLOBAL_VR_MOTION_SICKNESS_FOV{{System::Main, "VR", "MotionSicknessFOV"}, 0};
const ConfigInfo<int> GLOBAL_VR_PLAYER{{System::Main, "VR", "Player"}, 0};
const ConfigInfo<int> GLOBAL_VR_PLAYER_2{{System::Main, "VR", "Player2"}, 1};
const ConfigInfo<int> GLOBAL_VR_MIRROR_PLAYER{{System::Main, "VR", "MirrorPlayer"},
                                              VR_PLAYER_DEFAULT};
const ConfigInfo<int> GLOBAL_VR_MIRROR_STYLE{{System::Main, "VR", "MirrorStyle"}, VR_MIRROR_LEFT};
const ConfigInfo<float> GLOBAL_VR_TIMEWARP_TWEAK{{System::Main, "VR", "TimewarpTweak"},
                                                 DEFAULT_VR_TIMEWARP_TWEAK};
const ConfigInfo<u32> GLOBAL_VR_NUM_EXTRA_FRAMES{{System::Main, "VR", "NumExtraFrames"},
                                                 DEFAULT_VR_EXTRA_FRAMES};
const ConfigInfo<u32> GLOBAL_VR_NUM_EXTRA_VIDEO_LOOPS{{System::Main, "VR", "NumExtraVideoLoops"},
                                                      DEFAULT_VR_EXTRA_VIDEO_LOOPS};
const ConfigInfo<u32> GLOBAL_VR_NUM_EXTRA_VIDEO_LOOPS_DIVIDER{
    {System::Main, "VR", "NumExtraVideoLoopsDivider"}, DEFAULT_VR_EXTRA_VIDEO_LOOPS_DIVIDER};
const ConfigInfo<bool> GLOBAL_VR_STABILIZE_ROLL{{System::Main, "VR", "StabilizeRoll"}, true};
const ConfigInfo<bool> GLOBAL_VR_STABILIZE_PITCH{{System::Main, "VR", "StabilizePitch"}, true};
const ConfigInfo<bool> GLOBAL_VR_STABILIZE_YAW{{System::Main, "VR", "StabilizeYaw"}, false};
const ConfigInfo<bool> GLOBAL_VR_STABILIZE_X{{System::Main, "VR", "StabilizeX"}, false};
const ConfigInfo<bool> GLOBAL_VR_STABILIZE_Y{{System::Main, "VR", "StabilizeY"}, false};
const ConfigInfo<bool> GLOBAL_VR_STABILIZE_Z{{System::Main, "VR", "StabilizeZ"}, false};
const ConfigInfo<bool> GLOBAL_VR_KEYHOLE{{System::Main, "VR", "Keyhole"}, false};
const ConfigInfo<float> GLOBAL_VR_KEYHOLE_WIDTH{{System::Main, "VR", "KeyholeWidth"}, 45.0f};
const ConfigInfo<bool> GLOBAL_VR_KEYHOLE_SNAP{{System::Main, "VR", "KeyholeSnap"}, false};
const ConfigInfo<float> GLOBAL_VR_KEYHOLE_SIZE{{System::Main, "VR", "KeyholeSnapSize"}, 30.0f};
const ConfigInfo<bool> GLOBAL_VR_PULL_UP_20_FPS{{System::Main, "VR", "PullUp20fps"}, false};
const ConfigInfo<bool> GLOBAL_VR_PULL_UP_30_FPS{{System::Main, "VR", "PullUp30fps"}, false};
const ConfigInfo<bool> GLOBAL_VR_PULL_UP_60_FPS{{System::Main, "VR", "PullUp60fps"}, false};
const ConfigInfo<bool> GLOBAL_VR_PULL_UP_AUTO{{System::Main, "VR", "PullUpAuto"}, false};
const ConfigInfo<bool> GLOBAL_VR_OPCODE_REPLAY{{System::Main, "VR", "OpcodeReplay"}, false};
const ConfigInfo<bool> GLOBAL_VR_OPCODE_WARNING_DISABLE{
    {System::Main, "VR", "OpcodeWarningDisable"}, false};
const ConfigInfo<bool> GLOBAL_VR_REPLAY_VERTEX_DATA{{System::Main, "VR", "ReplayVertexData"},
                                                    false};
const ConfigInfo<bool> GLOBAL_VR_REPLAY_OTHER_DATA{{System::Main, "VR", "ReplayOtherData"}, false};
const ConfigInfo<bool> GLOBAL_VR_PULL_UP_20_FPS_TIMEWARP{
    {System::Main, "VR", "PullUp20fpsTimewarp"}, false};
const ConfigInfo<bool> GLOBAL_VR_PULL_UP_30_FPS_TIMEWARP{
    {System::Main, "VR", "PullUp30fpsTimewarp"}, false};
const ConfigInfo<bool> GLOBAL_VR_PULL_UP_60_FPS_TIMEWARP{
    {System::Main, "VR", "PullUp60fpsTimewarp"}, false};
const ConfigInfo<bool> GLOBAL_VR_PULL_UP_AUTO_TIMEWARP{{System::Main, "VR", "PullUpAutoTimewarp"},
                                                       false};
const ConfigInfo<bool> GLOBAL_VR_SYNCHRONOUS_TIMEWARP{{System::Main, "VR", "SynchronousTimewarp"},
                                                      false};

const ConfigInfo<std::string> GLOBAL_VR_LEFT_TEXTURE{{System::Main, "VR", "LeftTexture"}, ""};
const ConfigInfo<std::string> GLOBAL_VR_RIGHT_TEXTURE{{System::Main, "VR", "RightTexture"}, ""};
const ConfigInfo<std::string> GLOBAL_VR_GC_LEFT_TEXTURE{{System::Main, "VR", "GCLeftTexture"}, ""};
const ConfigInfo<std::string> GLOBAL_VR_GC_RIGHT_TEXTURE{{System::Main, "VR", "GCRightTexture"},
                                                         ""};

}  // namespace Config
