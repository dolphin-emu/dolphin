// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

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
  iAspectRatio = Config::Get(Config::GFX_ASPECT_RATIO);
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

  if (iEFBScale == SCALE_FORCE_INTEGRAL)
  {
    // Round down to multiple of native IR
    switch (Config::GetBase(Config::GFX_EFB_SCALE))
    {
    case SCALE_AUTO:
      iEFBScale = SCALE_AUTO_INTEGRAL;
      break;
    case SCALE_1_5X:
      iEFBScale = SCALE_1X;
      break;
    case SCALE_2_5X:
      iEFBScale = SCALE_2X;
      break;
    default:
      iEFBScale = Config::GetBase(Config::GFX_EFB_SCALE);
      break;
    }
  }

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

bool VideoConfig::IsStereoEnabled() const
{
  return iStereoMode > 0;
}

bool VideoConfig::IsMSAAEnabled() const
{
  return iMultisamples > 1;
}

bool VideoConfig::IsSSAAEnabled() const
{
  return iMultisamples > 1 && bSSAA && backend_info.bSupportsSSAA;
}

union HostConfigBits
{
  u32 bits;

  struct
  {
    u32 msaa : 1;
    u32 ssaa : 1;
    u32 stereo : 1;
    u32 wireframe : 1;
    u32 per_pixel_lighting : 1;
    u32 vertex_rounding : 1;
    u32 fast_depth_calc : 1;
    u32 bounding_box : 1;
    u32 backend_dual_source_blend : 1;
    u32 backend_geometry_shaders : 1;
    u32 backend_early_z : 1;
    u32 backend_bbox : 1;
    u32 backend_gs_instancing : 1;
    u32 backend_clip_control : 1;
    u32 backend_ssaa : 1;
    u32 backend_atomics : 1;
    u32 backend_depth_clamp : 1;
    u32 backend_reversed_depth_range : 1;
    u32 pad : 14;
  };
};

u32 VideoConfig::GetHostConfigBits() const
{
  HostConfigBits bits = {};
  bits.msaa = IsMSAAEnabled();
  bits.ssaa = IsSSAAEnabled();
  bits.stereo = IsStereoEnabled();
  bits.wireframe = bWireFrame;
  bits.per_pixel_lighting = bEnablePixelLighting;
  bits.vertex_rounding = UseVertexRounding();
  bits.fast_depth_calc = bFastDepthCalc;
  bits.bounding_box = bBBoxEnable;
  bits.backend_dual_source_blend = backend_info.bSupportsDualSourceBlend;
  bits.backend_geometry_shaders = backend_info.bSupportsGeometryShaders;
  bits.backend_early_z = backend_info.bSupportsEarlyZ;
  bits.backend_bbox = backend_info.bSupportsBBox;
  bits.backend_gs_instancing = backend_info.bSupportsGSInstancing;
  bits.backend_clip_control = backend_info.bSupportsClipControl;
  bits.backend_ssaa = backend_info.bSupportsSSAA;
  bits.backend_atomics = backend_info.bSupportsFragmentStoresAndAtomics;
  bits.backend_depth_clamp = backend_info.bSupportsDepthClamp;
  bits.backend_reversed_depth_range = backend_info.bSupportsReversedDepthRange;
  return bits.bits;
}

std::string VideoConfig::GetHostConfigFilename() const
{
  u32 bits = GetHostConfigBits();
  return StringFromFormat("%08X", bits);
}
