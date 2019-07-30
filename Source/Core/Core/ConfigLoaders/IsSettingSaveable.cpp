// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/IsSettingSaveable.h"

#include <algorithm>
#include <vector>

#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"

namespace ConfigLoaders
{
bool IsSettingSaveable(const Config::ConfigLocation& config_location)
{
  if (config_location.system == Config::System::Logger)
    return true;

  if (config_location.system == Config::System::Main && config_location.section == "NetPlay")
    return true;

  if (config_location.system == Config::System::Main && config_location.section == "General")
    return true;

  const static std::vector<Config::ConfigLocation> s_setting_saveable{
      // Main.Core

      Config::MAIN_DEFAULT_ISO.location,
      Config::MAIN_MEMCARD_A_PATH.location,
      Config::MAIN_MEMCARD_B_PATH.location,
      Config::MAIN_AUTO_DISC_CHANGE.location,
      Config::MAIN_SYNC_ON_SKIP_IDLE.location,
      Config::MAIN_OVERCLOCK_ENABLE.location,
      Config::MAIN_OVERCLOCK.location,
      Config::MAIN_JIT_FOLLOW_BRANCH.location,

      // Main.Controls

      Config::MAIN_IR_PITCH.location,
      Config::MAIN_IR_YAW.location,
      Config::MAIN_IR_VERTICAL_OFFSET.location,

      // Main.Display
#ifndef ANDROID
      Config::MAIN_FULLSCREEN_DISPLAY_RES.location,
      Config::MAIN_FULLSCREEN.location,
      Config::MAIN_RENDER_TO_MAIN.location,
      Config::MAIN_RENDER_WINDOW_AUTOSIZE.location,
      Config::MAIN_KEEP_WINDOW_ON_TOP.location,
      Config::MAIN_DISABLE_SCREENSAVER.location,
#endif

      // Graphics.Hardware

      Config::GFX_VSYNC.location,
      Config::GFX_ADAPTER.location,

      // Graphics.Settings

      Config::GFX_WIDESCREEN_HACK.location,
      Config::GFX_ASPECT_RATIO.location,
      Config::GFX_CROP.location,
      Config::GFX_DISPLAY_SCALE.location,
      Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES.location,
      Config::GFX_SHOW_FPS.location,
      Config::GFX_SHOW_NETPLAY_PING.location,
      Config::GFX_SHOW_NETPLAY_MESSAGES.location,
      Config::GFX_LOG_RENDER_TIME_TO_FILE.location,
      Config::GFX_OVERLAY_STATS.location,
      Config::GFX_OVERLAY_PROJ_STATS.location,
      Config::GFX_DUMP_TEXTURES.location,
      Config::GFX_HIRES_TEXTURES.location,
      Config::GFX_CACHE_HIRES_TEXTURES.location,
      Config::GFX_DUMP_EFB_TARGET.location,
      Config::GFX_DUMP_FRAMES_AS_IMAGES.location,
      Config::GFX_FREE_LOOK.location,
      Config::GFX_USE_FFV1.location,
      Config::GFX_DUMP_FORMAT.location,
      Config::GFX_DUMP_CODEC.location,
      Config::GFX_DUMP_ENCODER.location,
      Config::GFX_DUMP_PATH.location,
      Config::GFX_BITRATE_KBPS.location,
      Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS.location,
      Config::GFX_ENABLE_GPU_TEXTURE_DECODING.location,
      Config::GFX_ENABLE_PIXEL_LIGHTING.location,
      Config::GFX_FAST_DEPTH_CALC.location,
      Config::GFX_MSAA.location,
      Config::GFX_SSAA.location,
      Config::GFX_EFB_SCALE.location,
      Config::GFX_TEXFMT_OVERLAY_ENABLE.location,
      Config::GFX_TEXFMT_OVERLAY_CENTER.location,
      Config::GFX_ENABLE_WIREFRAME.location,
      Config::GFX_DISABLE_FOG.location,
      Config::GFX_BORDERLESS_FULLSCREEN.location,
      Config::GFX_ENABLE_VALIDATION_LAYER.location,
      Config::GFX_BACKEND_MULTITHREADING.location,
      Config::GFX_COMMAND_BUFFER_EXECUTE_INTERVAL.location,
      Config::GFX_SHADER_CACHE.location,
      Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING.location,
      Config::GFX_SHADER_COMPILATION_MODE.location,
      Config::GFX_SHADER_COMPILER_THREADS.location,
      Config::GFX_SHADER_PRECOMPILER_THREADS.location,

      Config::GFX_SW_ZCOMPLOC.location,
      Config::GFX_SW_ZFREEZE.location,
      Config::GFX_SW_DUMP_OBJECTS.location,
      Config::GFX_SW_DUMP_TEV_STAGES.location,
      Config::GFX_SW_DUMP_TEV_TEX_FETCHES.location,
      Config::GFX_SW_DRAW_START.location,
      Config::GFX_SW_DRAW_END.location,

      // Graphics.Enhancements

      Config::GFX_ENHANCE_FORCE_FILTERING.location,
      Config::GFX_ENHANCE_MAX_ANISOTROPY.location,
      Config::GFX_ENHANCE_POST_SHADER.location,
      Config::GFX_ENHANCE_FORCE_TRUE_COLOR.location,
      Config::GFX_ENHANCE_DISABLE_COPY_FILTER.location,
      Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION.location,

      // Graphics.Hacks

      Config::GFX_HACK_EFB_ACCESS_ENABLE.location,
      Config::GFX_HACK_EFB_DEFER_INVALIDATION.location,
      Config::GFX_HACK_EFB_ACCESS_TILE_SIZE.location,
      Config::GFX_HACK_BBOX_ENABLE.location,
      Config::GFX_HACK_FORCE_PROGRESSIVE.location,
      Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM.location,
      Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM.location,
      Config::GFX_HACK_DISABLE_COPY_TO_VRAM.location,
      Config::GFX_HACK_DEFER_EFB_COPIES.location,
      Config::GFX_HACK_IMMEDIATE_XFB.location,
      Config::GFX_HACK_COPY_EFB_SCALED.location,
      Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES.location,
      Config::GFX_HACK_TMEM_CACHE_EMULATION.location,
      Config::GFX_HACK_VERTEX_ROUDING.location,

      // Graphics.GameSpecific

      Config::GFX_PERF_QUERIES_ENABLE.location,

      // UI.General

      Config::MAIN_USE_DISCORD_PRESENCE.location,

  };

  return std::find(s_setting_saveable.begin(), s_setting_saveable.end(), config_location) !=
         s_setting_saveable.end();
}
}  // namespace ConfigLoaders
