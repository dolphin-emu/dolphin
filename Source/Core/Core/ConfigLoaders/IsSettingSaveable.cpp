// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/IsSettingSaveable.h"

#include <algorithm>
#include <vector>

#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"

namespace ConfigLoaders
{
bool IsSettingSaveable(const Config::ConfigLocation& config_location)
{
  if (config_location.system == Config::System::Logger)
    return true;

  if (config_location.system == Config::System::Main && config_location.section == "NetPlay")
    return true;

  const static std::vector<Config::ConfigLocation> s_setting_saveable{
      // Graphics.Hardware

      Config::GFX_VSYNC.location, Config::GFX_ADAPTER.location,

      // Graphics.Settings

      Config::GFX_WIDESCREEN_HACK.location, Config::GFX_ASPECT_RATIO.location,
      Config::GFX_CROP.location, Config::GFX_USE_XFB.location, Config::GFX_USE_REAL_XFB.location,
      Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES.location, Config::GFX_SHOW_FPS.location,
      Config::GFX_SHOW_NETPLAY_PING.location, Config::GFX_SHOW_NETPLAY_MESSAGES.location,
      Config::GFX_LOG_RENDER_TIME_TO_FILE.location, Config::GFX_OVERLAY_STATS.location,
      Config::GFX_OVERLAY_PROJ_STATS.location, Config::GFX_DUMP_TEXTURES.location,
      Config::GFX_HIRES_TEXTURES.location, Config::GFX_CONVERT_HIRES_TEXTURES.location,
      Config::GFX_CACHE_HIRES_TEXTURES.location, Config::GFX_DUMP_EFB_TARGET.location,
      Config::GFX_DUMP_FRAMES_AS_IMAGES.location, Config::GFX_FREE_LOOK.location,
      Config::GFX_USE_FFV1.location, Config::GFX_DUMP_FORMAT.location,
      Config::GFX_DUMP_CODEC.location, Config::GFX_DUMP_PATH.location,
      Config::GFX_BITRATE_KBPS.location, Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS.location,
      Config::GFX_ENABLE_GPU_TEXTURE_DECODING.location, Config::GFX_ENABLE_PIXEL_LIGHTING.location,
      Config::GFX_FAST_DEPTH_CALC.location, Config::GFX_MSAA.location, Config::GFX_SSAA.location,
      Config::GFX_EFB_SCALE.location, Config::GFX_TEXFMT_OVERLAY_ENABLE.location,
      Config::GFX_TEXFMT_OVERLAY_CENTER.location, Config::GFX_ENABLE_WIREFRAME.location,
      Config::GFX_DISABLE_FOG.location, Config::GFX_BORDERLESS_FULLSCREEN.location,
      Config::GFX_ENABLE_VALIDATION_LAYER.location, Config::GFX_BACKEND_MULTITHREADING.location,
      Config::GFX_COMMAND_BUFFER_EXECUTE_INTERVAL.location, Config::GFX_SHADER_CACHE.location,
      Config::GFX_BACKGROUND_SHADER_COMPILING.location,
      Config::GFX_DISABLE_SPECIALIZED_SHADERS.location,
      Config::GFX_PRECOMPILE_UBER_SHADERS.location, Config::GFX_SHADER_COMPILER_THREADS.location,
      Config::GFX_SHADER_PRECOMPILER_THREADS.location,

      Config::GFX_SW_ZCOMPLOC.location, Config::GFX_SW_ZFREEZE.location,
      Config::GFX_SW_DUMP_OBJECTS.location, Config::GFX_SW_DUMP_TEV_STAGES.location,
      Config::GFX_SW_DUMP_TEV_TEX_FETCHES.location, Config::GFX_SW_DRAW_START.location,
      Config::GFX_SW_DRAW_END.location,

      // Graphics.Enhancements

      Config::GFX_ENHANCE_FORCE_FILTERING.location, Config::GFX_ENHANCE_MAX_ANISOTROPY.location,
      Config::GFX_ENHANCE_POST_SHADER.location, Config::GFX_ENHANCE_FORCE_TRUE_COLOR.location,

      // Graphics.Stereoscopy

      Config::GFX_STEREO_MODE.location, Config::GFX_STEREO_DEPTH.location,
      Config::GFX_STEREO_CONVERGENCE_PERCENTAGE.location, Config::GFX_STEREO_SWAP_EYES.location,
      Config::GFX_STEREO_CONVERGENCE.location, Config::GFX_STEREO_EFB_MONO_DEPTH.location,
      Config::GFX_STEREO_DEPTH_PERCENTAGE.location,

      // Graphics.Hacks

      Config::GFX_HACK_EFB_ACCESS_ENABLE.location, Config::GFX_HACK_BBOX_ENABLE.location,
      Config::GFX_HACK_BBOX_PREFER_STENCIL_IMPLEMENTATION.location,
      Config::GFX_HACK_FORCE_PROGRESSIVE.location, Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM.location,
      Config::GFX_HACK_COPY_EFB_ENABLED.location,
      Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES.location,
      Config::GFX_HACK_VERTEX_ROUDING.location,

      // Graphics.GameSpecific

      Config::GFX_PROJECTION_HACK.location, Config::GFX_PROJECTION_HACK_SZNEAR.location,
      Config::GFX_PROJECTION_HACK_SZFAR.location, Config::GFX_PROJECTION_HACK_ZNEAR.location,
      Config::GFX_PROJECTION_HACK_ZFAR.location, Config::GFX_PERF_QUERIES_ENABLE.location,

      // Main.VR global settings

      Config::GLOBAL_VR_SCALE.location, Config::GLOBAL_VR_FREE_LOOK_SENSITIVITY.location,
      Config::GLOBAL_VR_LEAN_BACK_ANGLE.location, Config::GLOBAL_VR_ENABLE_VR.location,
      Config::GLOBAL_VR_LOW_PERSISTENCE.location, Config::GLOBAL_VR_DYNAMIC_PREDICTION.location,
      Config::GLOBAL_VR_NO_MIRROR_TO_WINDOW.location,
      Config::GLOBAL_VR_ORIENTATION_TRACKING.location,
      Config::GLOBAL_VR_MAG_YAW_CORRECTION.location, Config::GLOBAL_VR_POSITION_TRACKING.location,
      Config::GLOBAL_VR_CHROMATIC.location, Config::GLOBAL_VR_TIMEWARP.location,
      Config::GLOBAL_VR_VIGNETTE.location, Config::GLOBAL_VR_NO_RESTORE.location,
      Config::GLOBAL_VR_FLIP_VERTICAL.location, Config::GLOBAL_VR_SRGB.location,
      Config::GLOBAL_VR_OVERDRIVE.location, Config::GLOBAL_VR_HQ_DISTORTION.location,
      Config::GLOBAL_VR_DISABLE_NEAR_CLIPPING.location,
      Config::GLOBAL_VR_AUTO_PAIR_VIVE_CONTROLLERS.location, Config::GLOBAL_VR_SHOW_HANDS.location,
      Config::GLOBAL_VR_SHOW_FEET.location, Config::GLOBAL_VR_SHOW_CONTROLLER.location,
      Config::GLOBAL_VR_SHOW_LASER_POINTER.location, Config::GLOBAL_VR_SHOW_AIM_RECTANGLE.location,
      Config::GLOBAL_VR_SHOW_HUD_BOX.location, Config::GLOBAL_VR_SHOW_2D_SCREEN_BOX.location,
      Config::GLOBAL_VR_SHOW_SENSOR_BAR.location, Config::GLOBAL_VR_SHOW_GAME_CAMERA.location,
      Config::GLOBAL_VR_SHOW_GAME_FRUSTUM.location, Config::GLOBAL_VR_SHOW_TRACKING_CAMERA.location,
      Config::GLOBAL_VR_SHOW_TRACKING_VOLUME.location, Config::GLOBAL_VR_SHOW_BASE_STATION.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_ALWAYS.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_FREELOOK.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_2D.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_LEFT_STICK.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_RIGHT_STICK.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_DPAD.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_IR.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_METHOD.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_SKYBOX.location,
      Config::GLOBAL_VR_MOTION_SICKNESS_FOV.location, Config::GLOBAL_VR_PLAYER.location,
      Config::GLOBAL_VR_PLAYER_2.location, Config::GLOBAL_VR_MIRROR_PLAYER.location,
      Config::GLOBAL_VR_MIRROR_STYLE.location, Config::GLOBAL_VR_TIMEWARP_TWEAK.location,
      Config::GLOBAL_VR_NUM_EXTRA_FRAMES.location, Config::GLOBAL_VR_NUM_EXTRA_VIDEO_LOOPS.location,
      Config::GLOBAL_VR_NUM_EXTRA_VIDEO_LOOPS_DIVIDER.location,
      Config::GLOBAL_VR_STABILIZE_ROLL.location, Config::GLOBAL_VR_STABILIZE_PITCH.location,
      Config::GLOBAL_VR_STABILIZE_YAW.location, Config::GLOBAL_VR_STABILIZE_X.location,
      Config::GLOBAL_VR_STABILIZE_Y.location, Config::GLOBAL_VR_STABILIZE_Z.location,
      Config::GLOBAL_VR_KEYHOLE.location, Config::GLOBAL_VR_KEYHOLE_WIDTH.location,
      Config::GLOBAL_VR_KEYHOLE_SNAP.location, Config::GLOBAL_VR_KEYHOLE_SIZE.location,
      Config::GLOBAL_VR_PULL_UP_20_FPS.location, Config::GLOBAL_VR_PULL_UP_30_FPS.location,
      Config::GLOBAL_VR_PULL_UP_60_FPS.location, Config::GLOBAL_VR_PULL_UP_AUTO.location,
      Config::GLOBAL_VR_OPCODE_REPLAY.location, Config::GLOBAL_VR_OPCODE_WARNING_DISABLE.location,
      Config::GLOBAL_VR_REPLAY_VERTEX_DATA.location, Config::GLOBAL_VR_REPLAY_OTHER_DATA.location,
      Config::GLOBAL_VR_PULL_UP_20_FPS_TIMEWARP.location,
      Config::GLOBAL_VR_PULL_UP_30_FPS_TIMEWARP.location,
      Config::GLOBAL_VR_PULL_UP_60_FPS_TIMEWARP.location,
      Config::GLOBAL_VR_PULL_UP_AUTO_TIMEWARP.location,
      Config::GLOBAL_VR_SYNCHRONOUS_TIMEWARP.location, Config::GLOBAL_VR_LEFT_TEXTURE.location,
      Config::GLOBAL_VR_RIGHT_TEXTURE.location, Config::GLOBAL_VR_GC_LEFT_TEXTURE.location,
      Config::GLOBAL_VR_GC_RIGHT_TEXTURE.location,
  };

  return std::find(s_setting_saveable.begin(), s_setting_saveable.end(), config_location) !=
         s_setting_saveable.end();
}

bool IsSettingSaveableGameINI(const Config::ConfigLocation& config_location)
{
  const static std::vector<Config::ConfigLocation> s_setting_saveable_game_ini{
      // Graphics.Game Specific VR Settings

      Config::GFX_VR_DISABLE_3D.location,
      Config::GFX_VR_HUD_FULLSCREEN.location,
      Config::GFX_VR_HUD_ON_TOP.location,
      Config::GFX_VR_DONT_CLEAR_SCREEN.location,
      Config::GFX_VR_CAN_READ_CAMERA_ANGLES.location,
      Config::GFX_VR_DETECT_SKYBOX.location,
      Config::GFX_VR_UNITS_PER_METRE.location,
      Config::GFX_VR_HUD_THICKNESS.location,
      Config::GFX_VR_HUD_DISTANCE.location,
      Config::GFX_VR_HUD_3D_CLOSER.location,
      Config::GFX_VR_CAMERA_FORWARD.location,
      Config::GFX_VR_CAMERA_PITCH.location,
      Config::GFX_VR_AIM_DISTANCE.location,
      Config::GFX_VR_MIN_FOV.location,
      Config::GFX_VR_N64_FOV.location,
      Config::GFX_VR_SCREEN_HEIGHT.location,
      Config::GFX_VR_SCREEN_THICKNESS.location,
      Config::GFX_VR_SCREEN_DISTANCE.location,
      Config::GFX_VR_SCREEN_RIGHT.location,
      Config::GFX_VR_SCREEN_UP.location,
      Config::GFX_VR_SCREEN_PITCH.location,
      Config::GFX_VR_METROID_PRIME.location,
      Config::GFX_VR_TELESCOPE_EYE.location,
      Config::GFX_VR_TELESCOPE_MAX_FOV.location,
      Config::GFX_VR_READ_PITCH.location,
      Config::GFX_VR_CAMERA_MIN_POLY.location,
      Config::GFX_VR_HUD_DESP_POSITION_0.location,
      Config::GFX_VR_HUD_DESP_POSITION_1.location,
      Config::GFX_VR_HUD_DESP_POSITION_2.location,
  };

  return (std::find(s_setting_saveable_game_ini.begin(), s_setting_saveable_game_ini.end(),
                    config_location) != s_setting_saveable_game_ini.end()) ||
         IsSettingSaveable(config_location);
}

}  // namespace ConfigLoader
