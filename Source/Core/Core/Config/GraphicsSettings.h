// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Config/Config.h"

enum class AspectMode : int;
enum class ShaderCompilationMode : int;
enum class StereoMode : int;

namespace Config
{
// Configuration Information

// Graphics.Hardware

extern const ConfigInfo<bool> GFX_VSYNC;
extern const ConfigInfo<int> GFX_ADAPTER;

// Graphics.Settings

extern const ConfigInfo<bool> GFX_WIDESCREEN_HACK;
extern const ConfigInfo<AspectMode> GFX_ASPECT_RATIO;
extern const ConfigInfo<AspectMode> GFX_SUGGESTED_ASPECT_RATIO;
extern const ConfigInfo<float> GFX_DISPLAY_SCALE;
extern const ConfigInfo<bool> GFX_CROP;
extern const ConfigInfo<int> GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES;
extern const ConfigInfo<bool> GFX_SHOW_FPS;
extern const ConfigInfo<bool> GFX_SHOW_NETPLAY_PING;
extern const ConfigInfo<bool> GFX_SHOW_NETPLAY_MESSAGES;
extern const ConfigInfo<bool> GFX_LOG_RENDER_TIME_TO_FILE;
extern const ConfigInfo<bool> GFX_OVERLAY_STATS;
extern const ConfigInfo<bool> GFX_OVERLAY_PROJ_STATS;
extern const ConfigInfo<bool> GFX_DUMP_TEXTURES;
extern const ConfigInfo<bool> GFX_HIRES_TEXTURES;
extern const ConfigInfo<bool> GFX_CACHE_HIRES_TEXTURES;
extern const ConfigInfo<bool> GFX_DUMP_EFB_TARGET;
extern const ConfigInfo<bool> GFX_DUMP_XFB_TARGET;
extern const ConfigInfo<bool> GFX_DUMP_FRAMES_AS_IMAGES;
extern const ConfigInfo<bool> GFX_FREE_LOOK;
extern const ConfigInfo<bool> GFX_USE_FFV1;
extern const ConfigInfo<std::string> GFX_DUMP_FORMAT;
extern const ConfigInfo<std::string> GFX_DUMP_CODEC;
extern const ConfigInfo<std::string> GFX_DUMP_ENCODER;
extern const ConfigInfo<std::string> GFX_DUMP_PATH;
extern const ConfigInfo<int> GFX_BITRATE_KBPS;
extern const ConfigInfo<bool> GFX_INTERNAL_RESOLUTION_FRAME_DUMPS;
extern const ConfigInfo<bool> GFX_ENABLE_GPU_TEXTURE_DECODING;
extern const ConfigInfo<bool> GFX_ENABLE_PIXEL_LIGHTING;
extern const ConfigInfo<bool> GFX_FAST_DEPTH_CALC;
extern const ConfigInfo<int> GFX_MSAA;
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
extern const ConfigInfo<bool> GFX_WAIT_FOR_SHADERS_BEFORE_STARTING;
extern const ConfigInfo<ShaderCompilationMode> GFX_SHADER_COMPILATION_MODE;
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
extern const ConfigInfo<bool> GFX_ENHANCE_DISABLE_COPY_FILTER;
extern const ConfigInfo<bool> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION;
extern const ConfigInfo<float> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD;

// Graphics.Hacks

extern const ConfigInfo<bool> GFX_HACK_EFB_ACCESS_ENABLE;
extern const ConfigInfo<bool> GFX_HACK_EFB_DEFER_INVALIDATION;
extern const ConfigInfo<int> GFX_HACK_EFB_ACCESS_TILE_SIZE;
extern const ConfigInfo<bool> GFX_HACK_BBOX_ENABLE;
extern const ConfigInfo<bool> GFX_HACK_FORCE_PROGRESSIVE;
extern const ConfigInfo<bool> GFX_HACK_SKIP_EFB_COPY_TO_RAM;
extern const ConfigInfo<bool> GFX_HACK_SKIP_XFB_COPY_TO_RAM;
extern const ConfigInfo<bool> GFX_HACK_DISABLE_COPY_TO_VRAM;
extern const ConfigInfo<bool> GFX_HACK_DEFER_EFB_COPIES;
extern const ConfigInfo<bool> GFX_HACK_IMMEDIATE_XFB;
extern const ConfigInfo<bool> GFX_HACK_COPY_EFB_SCALED;
extern const ConfigInfo<bool> GFX_HACK_EFB_EMULATE_FORMAT_CHANGES;
extern const ConfigInfo<bool> GFX_HACK_TMEM_CACHE_EMULATION;
extern const ConfigInfo<bool> GFX_HACK_VERTEX_ROUDING;

// Graphics.GameSpecific

extern const ConfigInfo<bool> GFX_PERF_QUERIES_ENABLE;

}  // namespace Config
