// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/GraphicsSettings.h"

#include <optional>
#include <string>

#include "Common/Config/Config.h"
#include "Common/StringUtil.h"
#include "VideoCommon/VideoConfig.h"

namespace Config
{
std::optional<int> ConvertFromLegacyEFBScale(int efb_scale)
{
  // In game INIs, -1 was used as a special value meaning
  // "use the value from the base layer but round it to an integer scale".
  // We only support integer scales nowadays, so we can simply ignore -1
  // in game INIs in order to automatically use a previous layer's value.
  if (efb_scale < 0)
    return {};

  return efb_scale - (efb_scale > 0) - (efb_scale > 2) - (efb_scale > 4);
}

std::optional<int> ConvertFromLegacyEFBScale(const std::string& efb_scale)
{
  int efb_scale_int;
  if (!TryParse(efb_scale, &efb_scale_int))
    return {};
  return ConvertFromLegacyEFBScale(efb_scale_int);
}

int ConvertToLegacyEFBScale(int efb_scale)
{
  return efb_scale + (efb_scale >= 0) + (efb_scale > 1) + (efb_scale > 2);
}

std::optional<int> ConvertToLegacyEFBScale(const std::string& efb_scale)
{
  int efb_scale_int;
  if (!TryParse(efb_scale, &efb_scale_int))
    return {};
  return ConvertToLegacyEFBScale(efb_scale_int);
}

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
const ConfigInfo<int> GFX_EFB_SCALE{{System::GFX, "Settings", "EFBScale"}, 1};
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
}  // namespace Config
