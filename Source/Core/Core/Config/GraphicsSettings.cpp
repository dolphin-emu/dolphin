// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/GraphicsSettings.h"

#include <string>

#include "Common/Config/Config.h"
#include "VideoCommon/VideoConfig.h"

namespace Config
{
// Configuration Information

// Graphics.Hardware

const Info<bool> GFX_VSYNC{{System::GFX, "Hardware", "VSync"}, false};
const Info<int> GFX_ADAPTER{{System::GFX, "Hardware", "Adapter"}, 0};

// Graphics.Settings

const Info<bool> GFX_WIDESCREEN_HACK{{System::GFX, "Settings", "wideScreenHack"}, false};
const Info<AspectMode> GFX_ASPECT_RATIO{{System::GFX, "Settings", "AspectRatio"}, AspectMode::Auto};
const Info<AspectMode> GFX_SUGGESTED_ASPECT_RATIO{{System::GFX, "Settings", "SuggestedAspectRatio"},
                                                  AspectMode::Auto};
const Info<bool> GFX_CROP{{System::GFX, "Settings", "Crop"}, false};
const Info<int> GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES{
    {System::GFX, "Settings", "SafeTextureCacheColorSamples"}, 128};
const Info<bool> GFX_SHOW_FPS{{System::GFX, "Settings", "ShowFPS"}, false};
const Info<bool> GFX_SHOW_FTIMES{{System::GFX, "Settings", "ShowFTimes"}, false};
const Info<bool> GFX_SHOW_VPS{{System::GFX, "Settings", "ShowVPS"}, false};
const Info<bool> GFX_SHOW_VTIMES{{System::GFX, "Settings", "ShowVTimes"}, false};
const Info<bool> GFX_SHOW_GRAPHS{{System::GFX, "Settings", "ShowGraphs"}, false};
const Info<bool> GFX_SHOW_SPEED{{System::GFX, "Settings", "ShowSpeed"}, false};
const Info<bool> GFX_SHOW_SPEED_COLORS{{System::GFX, "Settings", "ShowSpeedColors"}, true};
const Info<int> GFX_PERF_SAMP_WINDOW{{System::GFX, "Settings", "PerfSampWindowMS"}, 1000};
const Info<bool> GFX_SHOW_NETPLAY_PING{{System::GFX, "Settings", "ShowNetPlayPing"}, false};
const Info<bool> GFX_SHOW_NETPLAY_MESSAGES{{System::GFX, "Settings", "ShowNetPlayMessages"}, false};
const Info<bool> GFX_LOG_RENDER_TIME_TO_FILE{{System::GFX, "Settings", "LogRenderTimeToFile"},
                                             false};
const Info<bool> GFX_OVERLAY_STATS{{System::GFX, "Settings", "OverlayStats"}, false};
const Info<bool> GFX_OVERLAY_PROJ_STATS{{System::GFX, "Settings", "OverlayProjStats"}, false};
const Info<bool> GFX_OVERLAY_SCISSOR_STATS{{System::GFX, "Settings", "OverlayScissorStats"}, false};
const Info<bool> GFX_DUMP_TEXTURES{{System::GFX, "Settings", "DumpTextures"}, false};
const Info<bool> GFX_DUMP_MIP_TEXTURES{{System::GFX, "Settings", "DumpMipTextures"}, true};
const Info<bool> GFX_DUMP_BASE_TEXTURES{{System::GFX, "Settings", "DumpBaseTextures"}, true};
const Info<bool> GFX_HIRES_TEXTURES{{System::GFX, "Settings", "HiresTextures"}, false};
const Info<bool> GFX_CACHE_HIRES_TEXTURES{{System::GFX, "Settings", "CacheHiresTextures"}, false};
const Info<bool> GFX_DUMP_EFB_TARGET{{System::GFX, "Settings", "DumpEFBTarget"}, false};
const Info<bool> GFX_DUMP_XFB_TARGET{{System::GFX, "Settings", "DumpXFBTarget"}, false};
const Info<bool> GFX_DUMP_FRAMES_AS_IMAGES{{System::GFX, "Settings", "DumpFramesAsImages"}, false};
const Info<bool> GFX_USE_FFV1{{System::GFX, "Settings", "UseFFV1"}, false};
const Info<std::string> GFX_DUMP_FORMAT{{System::GFX, "Settings", "DumpFormat"}, "avi"};
const Info<std::string> GFX_DUMP_CODEC{{System::GFX, "Settings", "DumpCodec"}, ""};
const Info<std::string> GFX_DUMP_PIXEL_FORMAT{{System::GFX, "Settings", "DumpPixelFormat"}, ""};
const Info<std::string> GFX_DUMP_ENCODER{{System::GFX, "Settings", "DumpEncoder"}, ""};
const Info<std::string> GFX_DUMP_PATH{{System::GFX, "Settings", "DumpPath"}, ""};
const Info<int> GFX_BITRATE_KBPS{{System::GFX, "Settings", "BitrateKbps"}, 25000};
const Info<bool> GFX_INTERNAL_RESOLUTION_FRAME_DUMPS{
    {System::GFX, "Settings", "InternalResolutionFrameDumps"}, false};
const Info<int> GFX_PNG_COMPRESSION_LEVEL{{System::GFX, "Settings", "PNGCompressionLevel"}, 6};
const Info<bool> GFX_ENABLE_GPU_TEXTURE_DECODING{
    {System::GFX, "Settings", "EnableGPUTextureDecoding"}, false};
const Info<bool> GFX_ENABLE_PIXEL_LIGHTING{{System::GFX, "Settings", "EnablePixelLighting"}, false};
const Info<bool> GFX_FAST_DEPTH_CALC{{System::GFX, "Settings", "FastDepthCalc"}, true};
const Info<u32> GFX_MSAA{{System::GFX, "Settings", "MSAA"}, 1};
const Info<bool> GFX_SSAA{{System::GFX, "Settings", "SSAA"}, false};
const Info<int> GFX_EFB_SCALE{{System::GFX, "Settings", "InternalResolution"}, 1};
const Info<int> GFX_MAX_EFB_SCALE{{System::GFX, "Settings", "MaxInternalResolution"}, 8};
const Info<bool> GFX_TEXFMT_OVERLAY_ENABLE{{System::GFX, "Settings", "TexFmtOverlayEnable"}, false};
const Info<bool> GFX_TEXFMT_OVERLAY_CENTER{{System::GFX, "Settings", "TexFmtOverlayCenter"}, false};
const Info<bool> GFX_ENABLE_WIREFRAME{{System::GFX, "Settings", "WireFrame"}, false};
const Info<bool> GFX_DISABLE_FOG{{System::GFX, "Settings", "DisableFog"}, false};
const Info<bool> GFX_BORDERLESS_FULLSCREEN{{System::GFX, "Settings", "BorderlessFullscreen"},
                                           false};
const Info<bool> GFX_ENABLE_VALIDATION_LAYER{{System::GFX, "Settings", "EnableValidationLayer"},
                                             false};

const Info<bool> GFX_BACKEND_MULTITHREADING{{System::GFX, "Settings", "BackendMultithreading"},
                                            true};
const Info<int> GFX_COMMAND_BUFFER_EXECUTE_INTERVAL{
    {System::GFX, "Settings", "CommandBufferExecuteInterval"}, 100};

const Info<bool> GFX_SHADER_CACHE{{System::GFX, "Settings", "ShaderCache"}, true};
const Info<bool> GFX_WAIT_FOR_SHADERS_BEFORE_STARTING{
    {System::GFX, "Settings", "WaitForShadersBeforeStarting"}, false};
const Info<ShaderCompilationMode> GFX_SHADER_COMPILATION_MODE{
    {System::GFX, "Settings", "ShaderCompilationMode"}, ShaderCompilationMode::Synchronous};
const Info<int> GFX_SHADER_COMPILER_THREADS{{System::GFX, "Settings", "ShaderCompilerThreads"}, 1};
const Info<int> GFX_SHADER_PRECOMPILER_THREADS{
    {System::GFX, "Settings", "ShaderPrecompilerThreads"}, -1};
const Info<bool> GFX_SAVE_TEXTURE_CACHE_TO_STATE{
    {System::GFX, "Settings", "SaveTextureCacheToState"}, true};
const Info<bool> GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION{
    {System::GFX, "Settings", "PreferVSForLinePointExpansion"}, false};
const Info<bool> GFX_CPU_CULL{{System::GFX, "Settings", "CPUCull"}, false};

const Info<TriState> GFX_MTL_MANUALLY_UPLOAD_BUFFERS{
    {System::GFX, "Settings", "ManuallyUploadBuffers"}, TriState::Auto};
const Info<TriState> GFX_MTL_USE_PRESENT_DRAWABLE{
    {System::GFX, "Settings", "MTLUsePresentDrawable"}, TriState::Auto};

const Info<bool> GFX_SW_DUMP_OBJECTS{{System::GFX, "Settings", "SWDumpObjects"}, false};
const Info<bool> GFX_SW_DUMP_TEV_STAGES{{System::GFX, "Settings", "SWDumpTevStages"}, false};
const Info<bool> GFX_SW_DUMP_TEV_TEX_FETCHES{{System::GFX, "Settings", "SWDumpTevTexFetches"},
                                             false};

const Info<bool> GFX_PREFER_GLES{{System::GFX, "Settings", "PreferGLES"}, false};

const Info<bool> GFX_MODS_ENABLE{{System::GFX, "Settings", "EnableMods"}, false};

// Graphics.Enhancements

const Info<TextureFilteringMode> GFX_ENHANCE_FORCE_TEXTURE_FILTERING{
    {System::GFX, "Enhancements", "ForceTextureFiltering"}, TextureFilteringMode::Default};
const Info<int> GFX_ENHANCE_MAX_ANISOTROPY{{System::GFX, "Enhancements", "MaxAnisotropy"}, 0};
const Info<std::string> GFX_ENHANCE_POST_SHADER{
    {System::GFX, "Enhancements", "PostProcessingShader"}, ""};
const Info<bool> GFX_ENHANCE_FORCE_TRUE_COLOR{{System::GFX, "Enhancements", "ForceTrueColor"},
                                              true};
const Info<bool> GFX_ENHANCE_DISABLE_COPY_FILTER{{System::GFX, "Enhancements", "DisableCopyFilter"},
                                                 true};
const Info<bool> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION{
    {System::GFX, "Enhancements", "ArbitraryMipmapDetection"}, true};
const Info<float> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD{
    {System::GFX, "Enhancements", "ArbitraryMipmapDetectionThreshold"}, 14.0f};

// Graphics.Stereoscopy

const Info<StereoMode> GFX_STEREO_MODE{{System::GFX, "Stereoscopy", "StereoMode"}, StereoMode::Off};
const Info<int> GFX_STEREO_DEPTH{{System::GFX, "Stereoscopy", "StereoDepth"}, 20};
const Info<int> GFX_STEREO_CONVERGENCE_PERCENTAGE{
    {System::GFX, "Stereoscopy", "StereoConvergencePercentage"}, 100};
const Info<bool> GFX_STEREO_SWAP_EYES{{System::GFX, "Stereoscopy", "StereoSwapEyes"}, false};
const Info<int> GFX_STEREO_CONVERGENCE{{System::GFX, "Stereoscopy", "StereoConvergence"}, 20};
const Info<bool> GFX_STEREO_EFB_MONO_DEPTH{{System::GFX, "Stereoscopy", "StereoEFBMonoDepth"},
                                           false};
const Info<int> GFX_STEREO_DEPTH_PERCENTAGE{{System::GFX, "Stereoscopy", "StereoDepthPercentage"},
                                            100};

// Graphics.Hacks

const Info<bool> GFX_HACK_EFB_ACCESS_ENABLE{{System::GFX, "Hacks", "EFBAccessEnable"}, true};
const Info<bool> GFX_HACK_EFB_DEFER_INVALIDATION{
    {System::GFX, "Hacks", "EFBAccessDeferInvalidation"}, false};
const Info<int> GFX_HACK_EFB_ACCESS_TILE_SIZE{{System::GFX, "Hacks", "EFBAccessTileSize"}, 64};
const Info<bool> GFX_HACK_BBOX_ENABLE{{System::GFX, "Hacks", "BBoxEnable"}, false};
const Info<bool> GFX_HACK_FORCE_PROGRESSIVE{{System::GFX, "Hacks", "ForceProgressive"}, true};
const Info<bool> GFX_HACK_SKIP_EFB_COPY_TO_RAM{{System::GFX, "Hacks", "EFBToTextureEnable"}, true};
const Info<bool> GFX_HACK_SKIP_XFB_COPY_TO_RAM{{System::GFX, "Hacks", "XFBToTextureEnable"}, true};
const Info<bool> GFX_HACK_DISABLE_COPY_TO_VRAM{{System::GFX, "Hacks", "DisableCopyToVRAM"}, false};
const Info<bool> GFX_HACK_DEFER_EFB_COPIES{{System::GFX, "Hacks", "DeferEFBCopies"}, true};
const Info<bool> GFX_HACK_IMMEDIATE_XFB{{System::GFX, "Hacks", "ImmediateXFBEnable"}, false};
const Info<bool> GFX_HACK_SKIP_DUPLICATE_XFBS{{System::GFX, "Hacks", "SkipDuplicateXFBs"}, true};
const Info<bool> GFX_HACK_EARLY_XFB_OUTPUT{{System::GFX, "Hacks", "EarlyXFBOutput"}, true};
const Info<bool> GFX_HACK_COPY_EFB_SCALED{{System::GFX, "Hacks", "EFBScaledCopy"}, true};
const Info<bool> GFX_HACK_EFB_EMULATE_FORMAT_CHANGES{
    {System::GFX, "Hacks", "EFBEmulateFormatChanges"}, false};
const Info<bool> GFX_HACK_VERTEX_ROUNDING{{System::GFX, "Hacks", "VertexRounding"}, false};
const Info<bool> GFX_HACK_VI_SKIP{{System::GFX, "Hacks", "VISkip"}, false};
const Info<u32> GFX_HACK_MISSING_COLOR_VALUE{{System::GFX, "Hacks", "MissingColorValue"},
                                             0xFFFFFFFF};
const Info<bool> GFX_HACK_FAST_TEXTURE_SAMPLING{{System::GFX, "Hacks", "FastTextureSampling"},
                                                true};
#ifdef __APPLE__
const Info<bool> GFX_HACK_NO_MIPMAPPING{{System::GFX, "Hacks", "NoMipmapping"}, false};
#endif

// Graphics.GameSpecific

const Info<bool> GFX_PERF_QUERIES_ENABLE{{System::GFX, "GameSpecific", "PerfQueriesEnable"}, false};
}  // namespace Config
