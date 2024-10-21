// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/Config/Config.h"

enum class AspectMode : int;
enum class ShaderCompilationMode : int;
enum class StereoMode : int;
enum class StereoPerEyeResolution : int;
enum class TextureFilteringMode : int;
enum class OutputResamplingMode : int;
enum class ColorCorrectionRegion : int;
enum class TriState : int;
enum class FrameDumpResolutionType : int;

namespace Config
{
// Configuration Information

// Graphics.Hardware

extern const Info<bool> GFX_VSYNC;
extern const Info<int> GFX_ADAPTER;

// Graphics.Settings

extern const Info<bool> GFX_WIDESCREEN_HACK;
extern const Info<AspectMode> GFX_ASPECT_RATIO;
extern const Info<int> GFX_CUSTOM_ASPECT_RATIO_WIDTH;
extern const Info<int> GFX_CUSTOM_ASPECT_RATIO_HEIGHT;
extern const Info<AspectMode> GFX_SUGGESTED_ASPECT_RATIO;
extern const Info<u32> GFX_WIDESCREEN_HEURISTIC_TRANSITION_THRESHOLD;
extern const Info<float> GFX_WIDESCREEN_HEURISTIC_ASPECT_RATIO_SLOP;
extern const Info<float> GFX_WIDESCREEN_HEURISTIC_STANDARD_RATIO;
extern const Info<float> GFX_WIDESCREEN_HEURISTIC_WIDESCREEN_RATIO;
extern const Info<bool> GFX_CROP;
extern const Info<int> GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES;
extern const Info<bool> GFX_SHOW_FPS;
extern const Info<bool> GFX_SHOW_FTIMES;
extern const Info<bool> GFX_SHOW_VPS;
extern const Info<bool> GFX_SHOW_VTIMES;
extern const Info<bool> GFX_SHOW_GRAPHS;
extern const Info<bool> GFX_SHOW_SPEED;
extern const Info<bool> GFX_SHOW_SPEED_COLORS;
extern const Info<int> GFX_PERF_SAMP_WINDOW;
extern const Info<bool> GFX_SHOW_NETPLAY_PING;
extern const Info<bool> GFX_SHOW_NETPLAY_MESSAGES;
extern const Info<bool> GFX_LOG_RENDER_TIME_TO_FILE;
extern const Info<bool> GFX_OVERLAY_STATS;
extern const Info<bool> GFX_OVERLAY_PROJ_STATS;
extern const Info<bool> GFX_OVERLAY_SCISSOR_STATS;
extern const Info<bool> GFX_DUMP_TEXTURES;
extern const Info<bool> GFX_DUMP_MIP_TEXTURES;
extern const Info<bool> GFX_DUMP_BASE_TEXTURES;
extern const Info<int> GFX_TEXTURE_PNG_COMPRESSION_LEVEL;
extern const Info<bool> GFX_HIRES_TEXTURES;
extern const Info<bool> GFX_CACHE_HIRES_TEXTURES;
extern const Info<bool> GFX_DUMP_EFB_TARGET;
extern const Info<bool> GFX_DUMP_XFB_TARGET;
extern const Info<bool> GFX_DUMP_FRAMES_AS_IMAGES;
extern const Info<bool> GFX_USE_FFV1;
extern const Info<std::string> GFX_DUMP_FORMAT;
extern const Info<std::string> GFX_DUMP_CODEC;
extern const Info<std::string> GFX_DUMP_PIXEL_FORMAT;
extern const Info<std::string> GFX_DUMP_ENCODER;
extern const Info<std::string> GFX_DUMP_PATH;
extern const Info<int> GFX_BITRATE_KBPS;
extern const Info<FrameDumpResolutionType> GFX_FRAME_DUMPS_RESOLUTION_TYPE;
extern const Info<int> GFX_PNG_COMPRESSION_LEVEL;
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
extern const Info<bool> GFX_ENABLE_BREADCRUMBS;
extern const Info<bool> GFX_BACKEND_MULTITHREADING;
extern const Info<int> GFX_COMMAND_BUFFER_EXECUTE_INTERVAL;
extern const Info<bool> GFX_SHADER_CACHE;
extern const Info<bool> GFX_WAIT_FOR_SHADERS_BEFORE_STARTING;
extern const Info<ShaderCompilationMode> GFX_SHADER_COMPILATION_MODE;
extern const Info<int> GFX_SHADER_COMPILER_THREADS;
extern const Info<int> GFX_SHADER_PRECOMPILER_THREADS;
extern const Info<bool> GFX_SAVE_TEXTURE_CACHE_TO_STATE;
extern const Info<bool> GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION;
extern const Info<bool> GFX_CPU_CULL;

extern const Info<TriState> GFX_MTL_MANUALLY_UPLOAD_BUFFERS;
extern const Info<TriState> GFX_MTL_USE_PRESENT_DRAWABLE;

extern const Info<bool> GFX_SW_DUMP_OBJECTS;
extern const Info<bool> GFX_SW_DUMP_TEV_STAGES;
extern const Info<bool> GFX_SW_DUMP_TEV_TEX_FETCHES;

extern const Info<bool> GFX_PREFER_GLES;

extern const Info<bool> GFX_MODS_ENABLE;

// Graphics.Enhancements

extern const Info<TextureFilteringMode> GFX_ENHANCE_FORCE_TEXTURE_FILTERING;
extern const Info<int> GFX_ENHANCE_MAX_ANISOTROPY;  // NOTE - this is x in (1 << x)
extern const Info<OutputResamplingMode> GFX_ENHANCE_OUTPUT_RESAMPLING;
extern const Info<std::string> GFX_ENHANCE_POST_SHADER;
extern const Info<bool> GFX_ENHANCE_FORCE_TRUE_COLOR;
extern const Info<bool> GFX_ENHANCE_DISABLE_COPY_FILTER;
extern const Info<bool> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION;
extern const Info<float> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD;
extern const Info<bool> GFX_ENHANCE_HDR_OUTPUT;

// Color.Correction

static constexpr float GFX_CC_GAME_GAMMA_MIN = 2.2f;
static constexpr float GFX_CC_GAME_GAMMA_MAX = 2.8f;

static constexpr float GFX_CC_DISPLAY_GAMMA_MIN = 2.2f;
static constexpr float GFX_CC_DISPLAY_GAMMA_MAX = 2.4f;

static constexpr float GFX_CC_HDR_PAPER_WHITE_NITS_MIN = 80.f;
static constexpr float GFX_CC_HDR_PAPER_WHITE_NITS_MAX = 500.f;

extern const Info<bool> GFX_CC_CORRECT_COLOR_SPACE;
extern const Info<ColorCorrectionRegion> GFX_CC_GAME_COLOR_SPACE;
extern const Info<bool> GFX_CC_CORRECT_GAMMA;
extern const Info<float> GFX_CC_GAME_GAMMA;
extern const Info<bool> GFX_CC_SDR_DISPLAY_GAMMA_SRGB;
extern const Info<float> GFX_CC_SDR_DISPLAY_CUSTOM_GAMMA;
extern const Info<float> GFX_CC_HDR_PAPER_WHITE_NITS;

// Graphics.Stereoscopy

extern const Info<StereoMode> GFX_STEREO_MODE;
extern const Info<bool> GFX_STEREO_PER_EYE_RESOLUTION_FULL;
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
extern const Info<bool> GFX_HACK_EARLY_XFB_OUTPUT;
extern const Info<bool> GFX_HACK_COPY_EFB_SCALED;
extern const Info<bool> GFX_HACK_EFB_EMULATE_FORMAT_CHANGES;
extern const Info<bool> GFX_HACK_VERTEX_ROUNDING;
extern const Info<bool> GFX_HACK_VI_SKIP;
extern const Info<u32> GFX_HACK_MISSING_COLOR_VALUE;
extern const Info<bool> GFX_HACK_FAST_TEXTURE_SAMPLING;
#ifdef __APPLE__
extern const Info<bool> GFX_HACK_NO_MIPMAPPING;
#endif

// Graphics.GameSpecific

extern const Info<bool> GFX_PERF_QUERIES_ENABLE;

// Android custom GPU drivers

extern const Info<std::string> GFX_DRIVER_LIB_NAME;

}  // namespace Config
