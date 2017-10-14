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

  };

  return std::find(s_setting_saveable.begin(), s_setting_saveable.end(), config_location) !=
         s_setting_saveable.end();
}
}  // namespace ConfigLoader
