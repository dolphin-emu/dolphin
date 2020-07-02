// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Config/Config.h"

enum class AspectMode : int;
enum class ShaderCompilationMode : int;
enum class StereoMode : int;
enum class FreelookControlType : int;

namespace Config
{
// Configuration Information

// Graphics.Hardware

extern const Info<bool> GFX_VSYNC;
extern const Info<int> GFX_ADAPTER;

// Graphics.Settings

extern const Info<bool> GFX_WIDESCREEN_HACK;
extern const Info<AspectMode> GFX_ASPECT_RATIO;
extern const Info<AspectMode> GFX_SUGGESTED_ASPECT_RATIO;
extern const Info<bool> GFX_CROP;
extern const Info<int> GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES;
extern const Info<bool> GFX_SHOW_FPS;
extern const Info<bool> GFX_SHOW_NETPLAY_PING;
extern const Info<bool> GFX_SHOW_NETPLAY_MESSAGES;
extern const Info<bool> GFX_LOG_RENDER_TIME_TO_FILE;
extern const Info<bool> GFX_OVERLAY_STATS;
extern const Info<bool> GFX_OVERLAY_PROJ_STATS;
extern const Info<bool> GFX_DUMP_TEXTURES;
extern const Info<bool> GFX_DUMP_MIP_TEXTURES;
extern const Info<bool> GFX_DUMP_BASE_TEXTURES;
extern const Info<bool> GFX_HIRES_TEXTURES;
extern const Info<bool> GFX_CACHE_HIRES_TEXTURES;
extern const Info<bool> GFX_DUMP_EFB_TARGET;
extern const Info<bool> GFX_DUMP_XFB_TARGET;
extern const Info<bool> GFX_DUMP_FRAMES_AS_IMAGES;
extern const Info<bool> GFX_FREE_LOOK;
extern const Info<FreelookControlType> GFX_FREE_LOOK_CONTROL_TYPE;
extern const Info<bool> GFX_USE_FFV1;
extern const Info<std::string> GFX_DUMP_FORMAT;
extern const Info<std::string> GFX_DUMP_CODEC;
extern const Info<std::string> GFX_DUMP_ENCODER;
extern const Info<std::string> GFX_DUMP_PATH;
extern const Info<int> GFX_BITRATE_KBPS;
extern const Info<bool> GFX_INTERNAL_RESOLUTION_FRAME_DUMPS;
extern const Info<bool> GFX_ENABLE_GPU_TEXTURE_DECODING;
extern const Info<bool> GFX_ENABLE_PIXEL_LIGHTING;
extern const Info<bool> GFX_FAST_DEPTH_CALC;
extern const Info<u32> GFX_MSAA;
extern const Info<bool> GFX_SSAA;
extern const Info<int> GFX_EFB_SCALE;
extern const Info<int> GFX_MAX_EFB_SCALE;
extern const Info<bool> GFX_TEXFMT_OVERLAY_ENABLE;
extern const Info<bool> GFX_TEXFMT_OVERLAY_CENTER;
extern const Info<bool> GFX_ENABLE_WIREFRAME;
extern const Info<bool> GFX_DISABLE_FOG;
extern const Info<bool> GFX_BORDERLESS_FULLSCREEN;
extern const Info<bool> GFX_ENABLE_VALIDATION_LAYER;
extern const Info<bool> GFX_BACKEND_MULTITHREADING;
extern const Info<int> GFX_COMMAND_BUFFER_EXECUTE_INTERVAL;
extern const Info<bool> GFX_SHADER_CACHE;
extern const Info<bool> GFX_WAIT_FOR_SHADERS_BEFORE_STARTING;
extern const Info<ShaderCompilationMode> GFX_SHADER_COMPILATION_MODE;
extern const Info<int> GFX_SHADER_COMPILER_THREADS;
extern const Info<int> GFX_SHADER_PRECOMPILER_THREADS;
extern const Info<bool> GFX_SAVE_TEXTURE_CACHE_TO_STATE;

extern const Info<bool> GFX_SW_ZCOMPLOC;
extern const Info<bool> GFX_SW_ZFREEZE;
extern const Info<bool> GFX_SW_DUMP_OBJECTS;
extern const Info<bool> GFX_SW_DUMP_TEV_STAGES;
extern const Info<bool> GFX_SW_DUMP_TEV_TEX_FETCHES;
extern const Info<int> GFX_SW_DRAW_START;
extern const Info<int> GFX_SW_DRAW_END;

extern const Info<bool> GFX_PREFER_GLES;

// Graphics.Enhancements

extern const Info<bool> GFX_ENHANCE_FORCE_FILTERING;
extern const Info<int> GFX_ENHANCE_MAX_ANISOTROPY;  // NOTE - this is x in (1 << x)
extern const Info<std::string> GFX_ENHANCE_POST_SHADER;
extern const Info<bool> GFX_ENHANCE_FORCE_TRUE_COLOR;
extern const Info<bool> GFX_ENHANCE_DISABLE_COPY_FILTER;
extern const Info<bool> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION;
extern const Info<float> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD;

// Graphics.Stereoscopy

extern const Info<StereoMode> GFX_STEREO_MODE;
extern const Info<int> GFX_STEREO_DEPTH;
extern const Info<int> GFX_STEREO_CONVERGENCE_PERCENTAGE;
extern const Info<bool> GFX_STEREO_SWAP_EYES;
extern const Info<int> GFX_STEREO_CONVERGENCE;
extern const Info<bool> GFX_STEREO_EFB_MONO_DEPTH;
extern const Info<int> GFX_STEREO_DEPTH_PERCENTAGE;

// Stereoscopy pseudo-limits for consistent behavior between enhancements tab and hotkeys.
static constexpr int GFX_STEREO_DEPTH_MAXIMUM = 100;
static constexpr int GFX_STEREO_CONVERGENCE_MAXIMUM = 200;

// Graphics.Hacks

extern const Info<bool> GFX_HACK_EFB_ACCESS_ENABLE;
extern const Info<bool> GFX_HACK_EFB_DEFER_INVALIDATION;
extern const Info<int> GFX_HACK_EFB_ACCESS_TILE_SIZE;
extern const Info<bool> GFX_HACK_BBOX_ENABLE;
extern const Info<bool> GFX_HACK_FORCE_PROGRESSIVE;
extern const Info<bool> GFX_HACK_SKIP_EFB_COPY_TO_RAM;
extern const Info<bool> GFX_HACK_SKIP_XFB_COPY_TO_RAM;
extern const Info<bool> GFX_HACK_DISABLE_COPY_TO_VRAM;
extern const Info<bool> GFX_HACK_DEFER_EFB_COPIES;
extern const Info<bool> GFX_HACK_IMMEDIATE_XFB;
extern const Info<bool> GFX_HACK_SKIP_DUPLICATE_XFBS;
extern const Info<bool> GFX_HACK_COPY_EFB_SCALED;
extern const Info<bool> GFX_HACK_EFB_EMULATE_FORMAT_CHANGES;
extern const Info<bool> GFX_HACK_VERTEX_ROUDING;
extern const Info<bool> GFX_HACK_FORCE_LOGICOPS_FALLBACK;

// Graphics.GameSpecific

extern const Info<bool> GFX_PERF_QUERIES_ENABLE;

}  // namespace Config
