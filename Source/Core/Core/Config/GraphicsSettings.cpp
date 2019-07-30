// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/GraphicsSettings.h"

#include <string>

#include "Common/Config/Config.h"
#include "VideoCommon/VideoConfig.h"

namespace Config
{
// Configuration Information

// Graphics.Hardware

const ConfigInfo<bool> GFX_VSYNC{{System::GFX, "Hardware", "VSync"}, false};
const ConfigInfo<int> GFX_ADAPTER{{System::GFX, "Hardware", "Adapter"}, 0};

// Graphics.Settings

const ConfigInfo<bool> GFX_WIDESCREEN_HACK{{System::GFX, "Settings", "wideScreenHack"}, false};
const ConfigInfo<AspectMode> GFX_ASPECT_RATIO{{System::GFX, "Settings", "AspectRatio"},
                                              AspectMode::Auto};
const ConfigInfo<AspectMode> GFX_SUGGESTED_ASPECT_RATIO{
    {System::GFX, "Settings", "SuggestedAspectRatio"}, AspectMode::Auto};
const ConfigInfo<bool> GFX_CROP{{System::GFX, "Settings", "Crop"}, false};
const ConfigInfo<float> GFX_DISPLAY_SCALE{{System::GFX, "Settings", "DisplayScale"}, 1.0f};
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
const ConfigInfo<bool> GFX_CACHE_HIRES_TEXTURES{{System::GFX, "Settings", "CacheHiresTextures"},
                                                false};
const ConfigInfo<bool> GFX_DUMP_EFB_TARGET{{System::GFX, "Settings", "DumpEFBTarget"}, false};
const ConfigInfo<bool> GFX_DUMP_XFB_TARGET{{System::GFX, "Settings", "DumpXFBTarget"}, false};
const ConfigInfo<bool> GFX_DUMP_FRAMES_AS_IMAGES{{System::GFX, "Settings", "DumpFramesAsImages"},
                                                 false};
const ConfigInfo<bool> GFX_FREE_LOOK{{System::GFX, "Settings", "FreeLook"}, false};
const ConfigInfo<bool> GFX_USE_FFV1{{System::GFX, "Settings", "UseFFV1"}, false};
const ConfigInfo<std::string> GFX_DUMP_FORMAT{{System::GFX, "Settings", "DumpFormat"}, "avi"};
const ConfigInfo<std::string> GFX_DUMP_CODEC{{System::GFX, "Settings", "DumpCodec"}, ""};
const ConfigInfo<std::string> GFX_DUMP_ENCODER{{System::GFX, "Settings", "DumpEncoder"}, ""};
const ConfigInfo<std::string> GFX_DUMP_PATH{{System::GFX, "Settings", "DumpPath"}, ""};
const ConfigInfo<int> GFX_BITRATE_KBPS{{System::GFX, "Settings", "BitrateKbps"}, 25000};
const ConfigInfo<bool> GFX_INTERNAL_RESOLUTION_FRAME_DUMPS{
    {System::GFX, "Settings", "InternalResolutionFrameDumps"}, false};
const ConfigInfo<bool> GFX_ENABLE_GPU_TEXTURE_DECODING{
    {System::GFX, "Settings", "EnableGPUTextureDecoding"}, false};
const ConfigInfo<bool> GFX_ENABLE_PIXEL_LIGHTING{{System::GFX, "Settings", "EnablePixelLighting"},
                                                 false};
const ConfigInfo<bool> GFX_FAST_DEPTH_CALC{{System::GFX, "Settings", "FastDepthCalc"}, true};
const ConfigInfo<int> GFX_MSAA{{System::GFX, "Settings", "MSAA"}, 1};
const ConfigInfo<bool> GFX_SSAA{{System::GFX, "Settings", "SSAA"}, false};
const ConfigInfo<int> GFX_EFB_SCALE{{System::GFX, "Settings", "InternalResolution"}, 1};
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
const ConfigInfo<bool> GFX_WAIT_FOR_SHADERS_BEFORE_STARTING{
    {System::GFX, "Settings", "WaitForShadersBeforeStarting"}, false};
const ConfigInfo<ShaderCompilationMode> GFX_SHADER_COMPILATION_MODE{
    {System::GFX, "Settings", "ShaderCompilationMode"}, ShaderCompilationMode::Synchronous};
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
const ConfigInfo<bool> GFX_ENHANCE_DISABLE_COPY_FILTER{
    {System::GFX, "Enhancements", "DisableCopyFilter"}, true};
const ConfigInfo<bool> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION{
    {System::GFX, "Enhancements", "ArbitraryMipmapDetection"}, true};
const ConfigInfo<float> GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD{
    {System::GFX, "Enhancements", "ArbitraryMipmapDetectionThreshold"}, 14.0f};

// Graphics.Hacks

const ConfigInfo<bool> GFX_HACK_EFB_ACCESS_ENABLE{{System::GFX, "Hacks", "EFBAccessEnable"}, false};
const ConfigInfo<bool> GFX_HACK_EFB_DEFER_INVALIDATION{
    {System::GFX, "Hacks", "EFBAccessDeferInvalidation"}, false};
const ConfigInfo<int> GFX_HACK_EFB_ACCESS_TILE_SIZE{{System::GFX, "Hacks", "EFBAccessTileSize"},
                                                    64};
const ConfigInfo<bool> GFX_HACK_BBOX_ENABLE{{System::GFX, "Hacks", "BBoxEnable"}, false};
const ConfigInfo<bool> GFX_HACK_FORCE_PROGRESSIVE{{System::GFX, "Hacks", "ForceProgressive"}, true};
const ConfigInfo<bool> GFX_HACK_SKIP_EFB_COPY_TO_RAM{{System::GFX, "Hacks", "EFBToTextureEnable"},
                                                     true};
const ConfigInfo<bool> GFX_HACK_SKIP_XFB_COPY_TO_RAM{{System::GFX, "Hacks", "XFBToTextureEnable"},
                                                     true};
const ConfigInfo<bool> GFX_HACK_DISABLE_COPY_TO_VRAM{{System::GFX, "Hacks", "DisableCopyToVRAM"},
                                                     false};
const ConfigInfo<bool> GFX_HACK_DEFER_EFB_COPIES{{System::GFX, "Hacks", "DeferEFBCopies"}, true};
const ConfigInfo<bool> GFX_HACK_IMMEDIATE_XFB{{System::GFX, "Hacks", "ImmediateXFBEnable"}, false};
const ConfigInfo<bool> GFX_HACK_COPY_EFB_SCALED{{System::GFX, "Hacks", "EFBScaledCopy"}, true};
const ConfigInfo<bool> GFX_HACK_EFB_EMULATE_FORMAT_CHANGES{
    {System::GFX, "Hacks", "EFBEmulateFormatChanges"}, false};
const ConfigInfo<bool> GFX_HACK_TMEM_CACHE_EMULATION{{System::GFX, "Hacks", "TMEMCacheEmulation"},
                                                     true};
const ConfigInfo<bool> GFX_HACK_VERTEX_ROUDING{{System::GFX, "Hacks", "VertexRounding"}, false};

// Graphics.GameSpecific

const ConfigInfo<bool> GFX_PERF_QUERIES_ENABLE{{System::GFX, "GameSpecific", "PerfQueriesEnable"},
                                               false};
}  // namespace Config
