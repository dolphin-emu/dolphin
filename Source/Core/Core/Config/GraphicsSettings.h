// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

#include "Common/Config/Config.h"

namespace Config
{
std::optional<int> ConvertFromLegacyEFBScale(int efb_scale);
std::optional<int> ConvertFromLegacyEFBScale(const std::string& efb_scale);
int ConvertToLegacyEFBScale(int efb_scale);
std::optional<int> ConvertToLegacyEFBScale(const std::string& efb_scale);

// Configuration Information

// Graphics.Hardware

extern const ConfigInfo<bool> GFX_VSYNC;
extern const ConfigInfo<int> GFX_ADAPTER;

// Graphics.Settings

extern const ConfigInfo<bool> GFX_WIDESCREEN_HACK;
extern const ConfigInfo<int> GFX_ASPECT_RATIO;
extern const ConfigInfo<int> GFX_SUGGESTED_ASPECT_RATIO;
extern const ConfigInfo<bool> GFX_CROP;
extern const ConfigInfo<bool> GFX_USE_XFB;
extern const ConfigInfo<bool> GFX_USE_REAL_XFB;
extern const ConfigInfo<int> GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES;
extern const ConfigInfo<bool> GFX_SHOW_FPS;
extern const ConfigInfo<bool> GFX_SHOW_NETPLAY_PING;
extern const ConfigInfo<bool> GFX_SHOW_NETPLAY_MESSAGES;
extern const ConfigInfo<bool> GFX_LOG_RENDER_TIME_TO_FILE;
extern const ConfigInfo<bool> GFX_OVERLAY_STATS;
extern const ConfigInfo<bool> GFX_OVERLAY_PROJ_STATS;
extern const ConfigInfo<bool> GFX_DUMP_TEXTURES;
extern const ConfigInfo<bool> GFX_HIRES_TEXTURES;
extern const ConfigInfo<bool> GFX_CONVERT_HIRES_TEXTURES;
extern const ConfigInfo<bool> GFX_CACHE_HIRES_TEXTURES;
extern const ConfigInfo<bool> GFX_DUMP_EFB_TARGET;
extern const ConfigInfo<bool> GFX_DUMP_FRAMES_AS_IMAGES;
extern const ConfigInfo<bool> GFX_FREE_LOOK;
extern const ConfigInfo<bool> GFX_USE_FFV1;
extern const ConfigInfo<std::string> GFX_DUMP_FORMAT;
extern const ConfigInfo<std::string> GFX_DUMP_CODEC;
extern const ConfigInfo<std::string> GFX_DUMP_PATH;
extern const ConfigInfo<int> GFX_BITRATE_KBPS;
extern const ConfigInfo<bool> GFX_INTERNAL_RESOLUTION_FRAME_DUMPS;
extern const ConfigInfo<bool> GFX_ENABLE_GPU_TEXTURE_DECODING;
extern const ConfigInfo<bool> GFX_ENABLE_PIXEL_LIGHTING;
extern const ConfigInfo<bool> GFX_FAST_DEPTH_CALC;
extern const ConfigInfo<u32> GFX_MSAA;
extern const ConfigInfo<bool> GFX_SSAA;
extern const ConfigInfo<int> GFX_EFB_SCALE;
extern const ConfigInfo<bool> GFX_TEXFMT_OVERLAY_ENABLE;
extern const ConfigInfo<bool> GFX_TEXFMT_OVERLAY_CENTER;
extern const ConfigInfo<bool> GFX_ENABLE_WIREFRAME;
extern const ConfigInfo<bool> GFX_DISABLE_FOG;
extern const ConfigInfo<bool> GFX_BORDERLESS_FULLSCREEN;
extern const ConfigInfo<bool> GFX_ENABLE_VALIDATION_LAYER;
extern const ConfigInfo<bool> GFX_BACKEND_MULTITHREADING;
extern const ConfigInfo<int> GFX_COMMAND_BUFFER_EXECUTE_INTERVAL;
extern const ConfigInfo<bool> GFX_SHADER_CACHE;
extern const ConfigInfo<bool> GFX_BACKGROUND_SHADER_COMPILING;
extern const ConfigInfo<bool> GFX_DISABLE_SPECIALIZED_SHADERS;
extern const ConfigInfo<bool> GFX_PRECOMPILE_UBER_SHADERS;
extern const ConfigInfo<int> GFX_SHADER_COMPILER_THREADS;
extern const ConfigInfo<int> GFX_SHADER_PRECOMPILER_THREADS;

extern const ConfigInfo<bool> GFX_SW_ZCOMPLOC;
extern const ConfigInfo<bool> GFX_SW_ZFREEZE;
extern const ConfigInfo<bool> GFX_SW_DUMP_OBJECTS;
extern const ConfigInfo<bool> GFX_SW_DUMP_TEV_STAGES;
extern const ConfigInfo<bool> GFX_SW_DUMP_TEV_TEX_FETCHES;
extern const ConfigInfo<int> GFX_SW_DRAW_START;
extern const ConfigInfo<int> GFX_SW_DRAW_END;

extern const ConfigInfo<bool> GFX_PREFER_GLES;

// Graphics.Enhancements

extern const ConfigInfo<bool> GFX_ENHANCE_FORCE_FILTERING;
extern const ConfigInfo<int> GFX_ENHANCE_MAX_ANISOTROPY;  // NOTE - this is x in (1 << x)
extern const ConfigInfo<std::string> GFX_ENHANCE_POST_SHADER;
extern const ConfigInfo<bool> GFX_ENHANCE_FORCE_TRUE_COLOR;

// Graphics.Stereoscopy

extern const ConfigInfo<int> GFX_STEREO_MODE;
extern const ConfigInfo<int> GFX_STEREO_DEPTH;
extern const ConfigInfo<int> GFX_STEREO_CONVERGENCE_PERCENTAGE;
extern const ConfigInfo<bool> GFX_STEREO_SWAP_EYES;
extern const ConfigInfo<int> GFX_STEREO_CONVERGENCE;
extern const ConfigInfo<bool> GFX_STEREO_EFB_MONO_DEPTH;
extern const ConfigInfo<int> GFX_STEREO_DEPTH_PERCENTAGE;

// Graphics.Hacks

extern const ConfigInfo<bool> GFX_HACK_EFB_ACCESS_ENABLE;
extern const ConfigInfo<bool> GFX_HACK_BBOX_ENABLE;
extern const ConfigInfo<bool> GFX_HACK_BBOX_PREFER_STENCIL_IMPLEMENTATION;
extern const ConfigInfo<bool> GFX_HACK_FORCE_PROGRESSIVE;
extern const ConfigInfo<bool> GFX_HACK_SKIP_EFB_COPY_TO_RAM;
extern const ConfigInfo<bool> GFX_HACK_COPY_EFB_ENABLED;
extern const ConfigInfo<bool> GFX_HACK_EFB_EMULATE_FORMAT_CHANGES;
extern const ConfigInfo<bool> GFX_HACK_VERTEX_ROUDING;

// Graphics.GameSpecific

extern const ConfigInfo<int> GFX_PROJECTION_HACK;
extern const ConfigInfo<int> GFX_PROJECTION_HACK_SZNEAR;
extern const ConfigInfo<int> GFX_PROJECTION_HACK_SZFAR;
extern const ConfigInfo<std::string> GFX_PROJECTION_HACK_ZNEAR;
extern const ConfigInfo<std::string> GFX_PROJECTION_HACK_ZFAR;
extern const ConfigInfo<bool> GFX_PERF_QUERIES_ENABLE;

}  // namespace Config
