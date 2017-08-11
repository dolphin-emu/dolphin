// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;
static bool s_has_registered_callback = false;

void UpdateActiveConfig()
{
  if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
    Movie::SetGraphicsConfig();
  g_ActiveConfig = g_Config;
}

VideoConfig::VideoConfig()
{
  // Needed for the first frame, I think
  fAspectRatioHackW = 1;
  fAspectRatioHackH = 1;

  // disable all features by default
  backend_info.api_type = APIType::Nothing;
  backend_info.MaxTextureSize = 16384;
  backend_info.bSupportsExclusiveFullscreen = false;
  backend_info.bSupportsMultithreading = false;
  backend_info.bSupportsInternalResolutionFrameDumps = false;
  backend_info.bSupportsST3CTextures = false;
  backend_info.bSupportsBPTCTextures = false;

  bEnableValidationLayer = false;
  bBackendMultithreading = true;
}

void VideoConfig::Refresh()
{
  if (!s_has_registered_callback)
  {
    Config::AddConfigChangedCallback([]() { g_Config.Refresh(); });
    s_has_registered_callback = true;
  }

  bVSync = Config::Get(Config::GFX_VSYNC);
  iAdapter = Config::Get(Config::GFX_ADAPTER);

  bWidescreenHack = Config::Get(Config::GFX_WIDESCREEN_HACK);
  const int aspect_ratio = Config::Get(Config::GFX_ASPECT_RATIO);
  if (aspect_ratio == ASPECT_AUTO)
    iAspectRatio = Config::Get(Config::GFX_SUGGESTED_ASPECT_RATIO);
  else
    iAspectRatio = aspect_ratio;
  bCrop = Config::Get(Config::GFX_CROP);
  bUseXFB = Config::Get(Config::GFX_USE_XFB);
  bUseRealXFB = Config::Get(Config::GFX_USE_REAL_XFB);
  iSafeTextureCache_ColorSamples = Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);
  bShowFPS = Config::Get(Config::GFX_SHOW_FPS);
  bShowNetPlayPing = Config::Get(Config::GFX_SHOW_NETPLAY_PING);
  bShowNetPlayMessages = Config::Get(Config::GFX_SHOW_NETPLAY_MESSAGES);
  bLogRenderTimeToFile = Config::Get(Config::GFX_LOG_RENDER_TIME_TO_FILE);
  bOverlayStats = Config::Get(Config::GFX_OVERLAY_STATS);
  bOverlayProjStats = Config::Get(Config::GFX_OVERLAY_PROJ_STATS);
  bDumpTextures = Config::Get(Config::GFX_DUMP_TEXTURES);
  bHiresTextures = Config::Get(Config::GFX_HIRES_TEXTURES);
  bConvertHiresTextures = Config::Get(Config::GFX_CONVERT_HIRES_TEXTURES);
  bCacheHiresTextures = Config::Get(Config::GFX_CACHE_HIRES_TEXTURES);
  bDumpEFBTarget = Config::Get(Config::GFX_DUMP_EFB_TARGET);
  bDumpFramesAsImages = Config::Get(Config::GFX_DUMP_FRAMES_AS_IMAGES);
  bFreeLook = Config::Get(Config::GFX_FREE_LOOK);
  bUseFFV1 = Config::Get(Config::GFX_USE_FFV1);
  sDumpFormat = Config::Get(Config::GFX_DUMP_FORMAT);
  sDumpCodec = Config::Get(Config::GFX_DUMP_CODEC);
  sDumpPath = Config::Get(Config::GFX_DUMP_PATH);
  iBitrateKbps = Config::Get(Config::GFX_BITRATE_KBPS);
  bInternalResolutionFrameDumps = Config::Get(Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS);
  bEnableGPUTextureDecoding = Config::Get(Config::GFX_ENABLE_GPU_TEXTURE_DECODING);
  bEnablePixelLighting = Config::Get(Config::GFX_ENABLE_PIXEL_LIGHTING);
  bFastDepthCalc = Config::Get(Config::GFX_FAST_DEPTH_CALC);
  iMultisamples = Config::Get(Config::GFX_MSAA);
  bSSAA = Config::Get(Config::GFX_SSAA);
  iEFBScale = Config::Get(Config::GFX_EFB_SCALE);
  bTexFmtOverlayEnable = Config::Get(Config::GFX_TEXFMT_OVERLAY_ENABLE);
  bTexFmtOverlayCenter = Config::Get(Config::GFX_TEXFMT_OVERLAY_CENTER);
  bWireFrame = Config::Get(Config::GFX_ENABLE_WIREFRAME);
  bDisableFog = Config::Get(Config::GFX_DISABLE_FOG);
  bBorderlessFullscreen = Config::Get(Config::GFX_BORDERLESS_FULLSCREEN);
  bEnableValidationLayer = Config::Get(Config::GFX_ENABLE_VALIDATION_LAYER);
  bBackendMultithreading = Config::Get(Config::GFX_BACKEND_MULTITHREADING);
  iCommandBufferExecuteInterval = Config::Get(Config::GFX_COMMAND_BUFFER_EXECUTE_INTERVAL);
  bShaderCache = Config::Get(Config::GFX_SHADER_CACHE);
  bBackgroundShaderCompiling = Config::Get(Config::GFX_BACKGROUND_SHADER_COMPILING);
  bDisableSpecializedShaders = Config::Get(Config::GFX_DISABLE_SPECIALIZED_SHADERS);
  bPrecompileUberShaders = Config::Get(Config::GFX_PRECOMPILE_UBER_SHADERS);
  iShaderCompilerThreads = Config::Get(Config::GFX_SHADER_COMPILER_THREADS);
  iShaderPrecompilerThreads = Config::Get(Config::GFX_SHADER_PRECOMPILER_THREADS);

  bZComploc = Config::Get(Config::GFX_SW_ZCOMPLOC);
  bZFreeze = Config::Get(Config::GFX_SW_ZFREEZE);
  bDumpObjects = Config::Get(Config::GFX_SW_DUMP_OBJECTS);
  bDumpTevStages = Config::Get(Config::GFX_SW_DUMP_TEV_STAGES);
  bDumpTevTextureFetches = Config::Get(Config::GFX_SW_DUMP_TEV_TEX_FETCHES);
  drawStart = Config::Get(Config::GFX_SW_DRAW_START);
  drawEnd = Config::Get(Config::GFX_SW_DRAW_END);

  bForceFiltering = Config::Get(Config::GFX_ENHANCE_FORCE_FILTERING);
  iMaxAnisotropy = Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY);
  sPostProcessingShader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  bForceTrueColor = Config::Get(Config::GFX_ENHANCE_FORCE_TRUE_COLOR);

  iStereoMode = Config::Get(Config::GFX_STEREO_MODE);
  iStereoDepth = Config::Get(Config::GFX_STEREO_DEPTH);
  iStereoConvergencePercentage = Config::Get(Config::GFX_STEREO_CONVERGENCE_PERCENTAGE);
  bStereoSwapEyes = Config::Get(Config::GFX_STEREO_SWAP_EYES);
  iStereoConvergence = Config::Get(Config::GFX_STEREO_CONVERGENCE);
  bStereoEFBMonoDepth = Config::Get(Config::GFX_STEREO_EFB_MONO_DEPTH);
  iStereoDepthPercentage = Config::Get(Config::GFX_STEREO_DEPTH_PERCENTAGE);

  bEFBAccessEnable = Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
  bBBoxEnable = Config::Get(Config::GFX_HACK_BBOX_ENABLE);
  bBBoxPreferStencilImplementation =
      Config::Get(Config::GFX_HACK_BBOX_PREFER_STENCIL_IMPLEMENTATION);
  bForceProgressive = Config::Get(Config::GFX_HACK_FORCE_PROGRESSIVE);
  bSkipEFBCopyToRam = Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
  bCopyEFBScaled = Config::Get(Config::GFX_HACK_COPY_EFB_ENABLED);
  bEFBEmulateFormatChanges = Config::Get(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES);
  bVertexRounding = Config::Get(Config::GFX_HACK_VERTEX_ROUDING);

  phack.m_enable = Config::Get(Config::GFX_PROJECTION_HACK) == 1;
  phack.m_sznear = Config::Get(Config::GFX_PROJECTION_HACK_SZNEAR) == 1;
  phack.m_szfar = Config::Get(Config::GFX_PROJECTION_HACK_SZFAR) == 1;
  phack.m_znear = Config::Get(Config::GFX_PROJECTION_HACK_ZNEAR);
  phack.m_zfar = Config::Get(Config::GFX_PROJECTION_HACK_ZFAR);
  bPerfQueriesEnable = Config::Get(Config::GFX_PERF_QUERIES_ENABLE);

  VerifyValidity();
}

void VideoConfig::VerifyValidity()
{
  // TODO: Check iMaxAnisotropy value
  if (iAdapter < 0 || iAdapter > ((int)backend_info.Adapters.size() - 1))
    iAdapter = 0;

  if (std::find(backend_info.AAModes.begin(), backend_info.AAModes.end(), iMultisamples) ==
      backend_info.AAModes.end())
    iMultisamples = 1;

  if (iStereoMode > 0)
  {
    if (!backend_info.bSupportsGeometryShaders)
    {
      OSD::AddMessage(
          "Stereoscopic 3D isn't supported by your GPU, support for OpenGL 3.2 is required.",
          10000);
      iStereoMode = 0;
    }

    if (bUseXFB && bUseRealXFB)
    {
      OSD::AddMessage("Stereoscopic 3D isn't supported with Real XFB, turning off stereoscopy.",
                      10000);
      iStereoMode = 0;
    }
  }
}

bool VideoConfig::IsVSync()
{
  return bVSync && !Core::GetIsThrottlerTempDisabled();
}

static u32 GetNumAutoShaderCompilerThreads()
{
  // Automatic number. We use clamp(cpus - 3, 1, 4).
  return static_cast<u32>(std::min(std::max(cpu_info.num_cores - 3, 1), 4));
}

u32 VideoConfig::GetShaderCompilerThreads() const
{
  if (iShaderCompilerThreads >= 0)
    return static_cast<u32>(iShaderCompilerThreads);
  else
    return GetNumAutoShaderCompilerThreads();
}

u32 VideoConfig::GetShaderPrecompilerThreads() const
{
  if (iShaderPrecompilerThreads >= 0)
    return static_cast<u32>(iShaderPrecompilerThreads);
  else
    return GetNumAutoShaderCompilerThreads();
}

bool VideoConfig::CanPrecompileUberShaders() const
{
  // We don't want to precompile ubershaders if they're never going to be used.
  return bPrecompileUberShaders && (bBackgroundShaderCompiling || bDisableSpecializedShaders);
}

bool VideoConfig::CanBackgroundCompileShaders() const
{
  // We require precompiled ubershaders to background compile shaders.
  return bBackgroundShaderCompiling && bPrecompileUberShaders;
}
