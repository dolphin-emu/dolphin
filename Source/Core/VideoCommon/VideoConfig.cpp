// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VideoConfig.h"

#include <algorithm>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/CPUThreadConfigCallback.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/FreeLookCamera.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexManagerBase.h"

#include "VideoCommon/VideoCommon.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;
static bool s_has_registered_callback = false;

static bool IsVSyncActive(const bool enabled)
{
  // Vsync is disabled when the throttler is disabled by the tab key.
  return enabled && !Core::GetIsThrottlerTempDisabled() &&
         Get(Config::MAIN_EMULATION_SPEED) == 1.0;
}

void UpdateActiveConfig()
{
  const auto& movie = Core::System::GetInstance().GetMovie();
  if (movie.IsPlayingInput() && movie.IsConfigSaved())
    movie.SetGraphicsConfig();
  g_ActiveConfig = g_Config;
  g_ActiveConfig.bVSyncActive = IsVSyncActive(g_ActiveConfig.bVSync);
}

void VideoConfig::Refresh()
{
  if (!s_has_registered_callback)
  {
    // There was a race condition between the video thread and the host thread here, if
    // corrections need to be made by VerifyValidity(). Briefly, the config will contain
    // invalid values. Instead, pause the video thread first, update the config and correct
    // it, then resume emulation, after which the video thread will detect the config has
    // changed and act accordingly.
    CPUThreadConfigCallback::AddConfigChangedCallback([]() {
      auto& system = Core::System::GetInstance();

      const bool lock_gpu_thread = IsRunning(system);
      if (lock_gpu_thread)
        system.GetFifo().PauseAndLock(true, false);

      g_Config.Refresh();
      g_Config.VerifyValidity();

      if (lock_gpu_thread)
        system.GetFifo().PauseAndLock(false, true);
    });
    s_has_registered_callback = true;
  }

  bVSync = Get(Config::GFX_VSYNC);
  iAdapter = Get(Config::GFX_ADAPTER);
  iManuallyUploadBuffers = Get(Config::GFX_MTL_MANUALLY_UPLOAD_BUFFERS);
  iUsePresentDrawable = Get(Config::GFX_MTL_USE_PRESENT_DRAWABLE);

  bWidescreenHack = Get(Config::GFX_WIDESCREEN_HACK);
  aspect_mode = Get(Config::GFX_ASPECT_RATIO);
  custom_aspect_width = Get(Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH);
  custom_aspect_height = Get(Config::GFX_CUSTOM_ASPECT_RATIO_HEIGHT);
  suggested_aspect_mode = Get(Config::GFX_SUGGESTED_ASPECT_RATIO);
  widescreen_heuristic_transition_threshold =
      Get(Config::GFX_WIDESCREEN_HEURISTIC_TRANSITION_THRESHOLD);
  widescreen_heuristic_aspect_ratio_slop =
      Get(Config::GFX_WIDESCREEN_HEURISTIC_ASPECT_RATIO_SLOP);
  widescreen_heuristic_standard_ratio =
      Get(Config::GFX_WIDESCREEN_HEURISTIC_STANDARD_RATIO);
  widescreen_heuristic_widescreen_ratio =
      Get(Config::GFX_WIDESCREEN_HEURISTIC_WIDESCREEN_RATIO);
  bCrop = Get(Config::GFX_CROP);
  iSafeTextureCache_ColorSamples = Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);
  bShowFPS = Get(Config::GFX_SHOW_FPS);
  bShowFTimes = Get(Config::GFX_SHOW_FTIMES);
  bShowVPS = Get(Config::GFX_SHOW_VPS);
  bShowVTimes = Get(Config::GFX_SHOW_VTIMES);
  bShowGraphs = Get(Config::GFX_SHOW_GRAPHS);
  bShowSpeed = Get(Config::GFX_SHOW_SPEED);
  bShowSpeedColors = Get(Config::GFX_SHOW_SPEED_COLORS);
  iPerfSampleUSec = Get(Config::GFX_PERF_SAMP_WINDOW) * 1000;
  bShowNetPlayPing = Get(Config::GFX_SHOW_NETPLAY_PING);
  bShowNetPlayMessages = Get(Config::GFX_SHOW_NETPLAY_MESSAGES);
  bLogRenderTimeToFile = Get(Config::GFX_LOG_RENDER_TIME_TO_FILE);
  bOverlayStats = Get(Config::GFX_OVERLAY_STATS);
  bOverlayProjStats = Get(Config::GFX_OVERLAY_PROJ_STATS);
  bOverlayScissorStats = Get(Config::GFX_OVERLAY_SCISSOR_STATS);
  bDumpTextures = Get(Config::GFX_DUMP_TEXTURES);
  bDumpMipmapTextures = Get(Config::GFX_DUMP_MIP_TEXTURES);
  bDumpBaseTextures = Get(Config::GFX_DUMP_BASE_TEXTURES);
  bHiresTextures = Get(Config::GFX_HIRES_TEXTURES);
  bCacheHiresTextures = Get(Config::GFX_CACHE_HIRES_TEXTURES);
  bDumpEFBTarget = Get(Config::GFX_DUMP_EFB_TARGET);
  bDumpXFBTarget = Get(Config::GFX_DUMP_XFB_TARGET);
  bDumpFramesAsImages = Get(Config::GFX_DUMP_FRAMES_AS_IMAGES);
  bUseFFV1 = Get(Config::GFX_USE_FFV1);
  sDumpFormat = Get(Config::GFX_DUMP_FORMAT);
  sDumpCodec = Get(Config::GFX_DUMP_CODEC);
  sDumpPixelFormat = Get(Config::GFX_DUMP_PIXEL_FORMAT);
  sDumpEncoder = Get(Config::GFX_DUMP_ENCODER);
  sDumpPath = Get(Config::GFX_DUMP_PATH);
  iBitrateKbps = Get(Config::GFX_BITRATE_KBPS);
  frame_dumps_resolution_type = Get(Config::GFX_FRAME_DUMPS_RESOLUTION_TYPE);
  bEnableGPUTextureDecoding = Get(Config::GFX_ENABLE_GPU_TEXTURE_DECODING);
  bPreferVSForLinePointExpansion = Get(Config::GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION);
  bEnablePixelLighting = Get(Config::GFX_ENABLE_PIXEL_LIGHTING);
  bFastDepthCalc = Get(Config::GFX_FAST_DEPTH_CALC);
  iMultisamples = Get(Config::GFX_MSAA);
  bSSAA = Get(Config::GFX_SSAA);
  iEFBScale = Get(Config::GFX_EFB_SCALE);
  bTexFmtOverlayEnable = Get(Config::GFX_TEXFMT_OVERLAY_ENABLE);
  bTexFmtOverlayCenter = Get(Config::GFX_TEXFMT_OVERLAY_CENTER);
  bWireFrame = Get(Config::GFX_ENABLE_WIREFRAME);
  bDisableFog = Get(Config::GFX_DISABLE_FOG);
  bBorderlessFullscreen = Get(Config::GFX_BORDERLESS_FULLSCREEN);
  bEnableValidationLayer = Get(Config::GFX_ENABLE_VALIDATION_LAYER);
  bBackendMultithreading = Get(Config::GFX_BACKEND_MULTITHREADING);
  iCommandBufferExecuteInterval = Get(Config::GFX_COMMAND_BUFFER_EXECUTE_INTERVAL);
  bShaderCache = Get(Config::GFX_SHADER_CACHE);
  bWaitForShadersBeforeStarting = Get(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING);
  iShaderCompilationMode = Get(Config::GFX_SHADER_COMPILATION_MODE);
  iShaderCompilerThreads = Get(Config::GFX_SHADER_COMPILER_THREADS);
  iShaderPrecompilerThreads = Get(Config::GFX_SHADER_PRECOMPILER_THREADS);
  bCPUCull = Get(Config::GFX_CPU_CULL);

  texture_filtering_mode = Get(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING);
  iMaxAnisotropy = Get(Config::GFX_ENHANCE_MAX_ANISOTROPY);
  output_resampling_mode = Get(Config::GFX_ENHANCE_OUTPUT_RESAMPLING);
  sPostProcessingShader = Get(Config::GFX_ENHANCE_POST_SHADER);
  bForceTrueColor = Get(Config::GFX_ENHANCE_FORCE_TRUE_COLOR);
  bDisableCopyFilter = Get(Config::GFX_ENHANCE_DISABLE_COPY_FILTER);
  bArbitraryMipmapDetection = Get(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION);
  fArbitraryMipmapDetectionThreshold =
      Get(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD);
  bHDR = Get(Config::GFX_ENHANCE_HDR_OUTPUT);

  color_correction.bCorrectColorSpace = Get(Config::GFX_CC_CORRECT_COLOR_SPACE);
  color_correction.game_color_space = Get(Config::GFX_CC_GAME_COLOR_SPACE);
  color_correction.bCorrectGamma = Get(Config::GFX_CC_CORRECT_GAMMA);
  color_correction.fGameGamma = Get(Config::GFX_CC_GAME_GAMMA);
  color_correction.bSDRDisplayGammaSRGB = Get(Config::GFX_CC_SDR_DISPLAY_GAMMA_SRGB);
  color_correction.fSDRDisplayCustomGamma = Get(Config::GFX_CC_SDR_DISPLAY_CUSTOM_GAMMA);
  color_correction.fHDRPaperWhiteNits = Get(Config::GFX_CC_HDR_PAPER_WHITE_NITS);

  stereo_mode = Get(Config::GFX_STEREO_MODE);
  iStereoDepth = Get(Config::GFX_STEREO_DEPTH);
  iStereoConvergencePercentage = Get(Config::GFX_STEREO_CONVERGENCE_PERCENTAGE);
  bStereoSwapEyes = Get(Config::GFX_STEREO_SWAP_EYES);
  iStereoConvergence = Get(Config::GFX_STEREO_CONVERGENCE);
  bStereoEFBMonoDepth = Get(Config::GFX_STEREO_EFB_MONO_DEPTH);
  iStereoDepthPercentage = Get(Config::GFX_STEREO_DEPTH_PERCENTAGE);

  bEFBAccessEnable = Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
  bEFBAccessDeferInvalidation = Get(Config::GFX_HACK_EFB_DEFER_INVALIDATION);
  bBBoxEnable = Get(Config::GFX_HACK_BBOX_ENABLE);
  bForceProgressive = Get(Config::GFX_HACK_FORCE_PROGRESSIVE);
  bSkipEFBCopyToRam = Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
  bSkipXFBCopyToRam = Get(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM);
  bDisableCopyToVRAM = Get(Config::GFX_HACK_DISABLE_COPY_TO_VRAM);
  bDeferEFBCopies = Get(Config::GFX_HACK_DEFER_EFB_COPIES);
  bImmediateXFB = Get(Config::GFX_HACK_IMMEDIATE_XFB);
  bVISkip = Get(Config::GFX_HACK_VI_SKIP);
  bSkipPresentingDuplicateXFBs = bVISkip || Get(Config::GFX_HACK_SKIP_DUPLICATE_XFBS);
  bCopyEFBScaled = Get(Config::GFX_HACK_COPY_EFB_SCALED);
  bEFBEmulateFormatChanges = Get(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES);
  bVertexRounding = Get(Config::GFX_HACK_VERTEX_ROUNDING);
  iEFBAccessTileSize = Get(Config::GFX_HACK_EFB_ACCESS_TILE_SIZE);
  iMissingColorValue = Get(Config::GFX_HACK_MISSING_COLOR_VALUE);
  bFastTextureSampling = Get(Config::GFX_HACK_FAST_TEXTURE_SAMPLING);
#ifdef __APPLE__
  bNoMipmapping = Config::Get(Config::GFX_HACK_NO_MIPMAPPING);
#endif

  bPerfQueriesEnable = Get(Config::GFX_PERF_QUERIES_ENABLE);

  bGraphicMods = Get(Config::GFX_MODS_ENABLE);

  customDriverLibraryName = Get(Config::GFX_DRIVER_LIB_NAME);
}

void VideoConfig::VerifyValidity()
{
  // TODO: Check iMaxAnisotropy value
  if (iAdapter < 0 || iAdapter > (static_cast<int>(backend_info.Adapters.size()) - 1))
    iAdapter = 0;

  if (std::ranges::find(backend_info.AAModes, iMultisamples) == backend_info.AAModes.end())
    iMultisamples = 1;

  if (stereo_mode != StereoMode::Off)
  {
    if (!backend_info.bSupportsGeometryShaders)
    {
      OSD::AddMessage(
          "Stereoscopic 3D isn't supported by your GPU, support for OpenGL 3.2 is required.",
          10000);
      stereo_mode = StereoMode::Off;
    }
  }
}

bool VideoConfig::UsingUberShaders() const
{
  return iShaderCompilationMode == ShaderCompilationMode::SynchronousUberShaders ||
         iShaderCompilationMode == ShaderCompilationMode::AsynchronousUberShaders;
}

static u32 GetNumAutoShaderCompilerThreads()
{
  // Automatic number.
  return static_cast<u32>(std::clamp(cpu_info.num_cores - 3, 1, 4));
}

static u32 GetNumAutoShaderPreCompilerThreads()
{
  // Automatic number. We use clamp(cpus - 2, 1, infty) here.
  // We chose this because we don't want to limit our speed-up
  // and at the same time leave two logical cores for the dolphin UI and the rest of the OS.
  return static_cast<u32>(std::max(cpu_info.num_cores - 2, 1));
}

u32 VideoConfig::GetShaderCompilerThreads() const
{
  if (!backend_info.bSupportsBackgroundCompiling)
    return 0;

  if (iShaderCompilerThreads >= 0)
    return static_cast<u32>(iShaderCompilerThreads);
  return GetNumAutoShaderCompilerThreads();
}

u32 VideoConfig::GetShaderPrecompilerThreads() const
{
  // When using background compilation, always keep the same thread count.
  if (!bWaitForShadersBeforeStarting)
    return GetShaderCompilerThreads();

  if (!backend_info.bSupportsBackgroundCompiling)
    return 0;

  if (iShaderPrecompilerThreads >= 0)
    return static_cast<u32>(iShaderPrecompilerThreads);
  if (!HasBug(DriverDetails::BUG_BROKEN_MULTITHREADED_SHADER_PRECOMPILATION))
    return GetNumAutoShaderPreCompilerThreads();
  return 1;
}

void CheckForConfigChanges()
{
  const ShaderHostConfig old_shader_host_config = ShaderHostConfig::GetCurrent();
  const StereoMode old_stereo = g_ActiveConfig.stereo_mode;
  const u32 old_multisamples = g_ActiveConfig.iMultisamples;
  const int old_anisotropy = g_ActiveConfig.iMaxAnisotropy;
  const int old_efb_access_tile_size = g_ActiveConfig.iEFBAccessTileSize;
  const auto old_texture_filtering_mode = g_ActiveConfig.texture_filtering_mode;
  const bool old_vsync = g_ActiveConfig.bVSyncActive;
  const bool old_bbox = g_ActiveConfig.bBBoxEnable;
  const int old_efb_scale = g_ActiveConfig.iEFBScale;
  const u32 old_game_mod_changes =
      g_ActiveConfig.graphics_mod_config ? g_ActiveConfig.graphics_mod_config->GetChangeCount() : 0;
  const bool old_graphics_mods_enabled = g_ActiveConfig.bGraphicMods;
  const AspectMode old_aspect_mode = g_ActiveConfig.aspect_mode;
  const AspectMode old_suggested_aspect_mode = g_ActiveConfig.suggested_aspect_mode;
  const bool old_widescreen_hack = g_ActiveConfig.bWidescreenHack;
  const auto old_post_processing_shader = g_ActiveConfig.sPostProcessingShader;
  const auto old_hdr = g_ActiveConfig.bHDR;

  UpdateActiveConfig();
  FreeLook::UpdateActiveConfig();
  g_vertex_manager->OnConfigChange();

  g_freelook_camera.SetControlType(FreeLook::GetActiveConfig().camera_config.control_type);

  if (g_ActiveConfig.bGraphicMods && !old_graphics_mods_enabled)
  {
    g_ActiveConfig.graphics_mod_config = GraphicsModGroupConfig(SConfig::GetInstance().GetGameID());
    g_ActiveConfig.graphics_mod_config->Load();
  }

  if (g_ActiveConfig.graphics_mod_config &&
      (old_game_mod_changes != g_ActiveConfig.graphics_mod_config->GetChangeCount()))
  {
    g_graphics_mod_manager->Load(*g_ActiveConfig.graphics_mod_config);
  }

  // Update texture cache settings with any changed options.
  g_texture_cache->OnConfigChanged(g_ActiveConfig);

  // EFB tile cache doesn't need to notify the backend.
  if (old_efb_access_tile_size != g_ActiveConfig.iEFBAccessTileSize)
    g_framebuffer_manager->SetEFBCacheTileSize(std::max(g_ActiveConfig.iEFBAccessTileSize, 0));

  // Determine which (if any) settings have changed.
  ShaderHostConfig new_host_config = ShaderHostConfig::GetCurrent();
  u32 changed_bits = 0;
  if (old_shader_host_config.bits != new_host_config.bits)
    changed_bits |= CONFIG_CHANGE_BIT_HOST_CONFIG;
  if (old_stereo != g_ActiveConfig.stereo_mode)
    changed_bits |= CONFIG_CHANGE_BIT_STEREO_MODE;
  if (old_multisamples != g_ActiveConfig.iMultisamples)
    changed_bits |= CONFIG_CHANGE_BIT_MULTISAMPLES;
  if (old_anisotropy != g_ActiveConfig.iMaxAnisotropy)
    changed_bits |= CONFIG_CHANGE_BIT_ANISOTROPY;
  if (old_texture_filtering_mode != g_ActiveConfig.texture_filtering_mode)
    changed_bits |= CONFIG_CHANGE_BIT_FORCE_TEXTURE_FILTERING;
  if (old_vsync != g_ActiveConfig.bVSyncActive)
    changed_bits |= CONFIG_CHANGE_BIT_VSYNC;
  if (old_bbox != g_ActiveConfig.bBBoxEnable)
    changed_bits |= CONFIG_CHANGE_BIT_BBOX;
  if (old_efb_scale != g_ActiveConfig.iEFBScale)
    changed_bits |= CONFIG_CHANGE_BIT_TARGET_SIZE;
  if (old_aspect_mode != g_ActiveConfig.aspect_mode)
    changed_bits |= CONFIG_CHANGE_BIT_ASPECT_RATIO;
  if (old_suggested_aspect_mode != g_ActiveConfig.suggested_aspect_mode)
    changed_bits |= CONFIG_CHANGE_BIT_ASPECT_RATIO;
  if (old_widescreen_hack != g_ActiveConfig.bWidescreenHack)
    changed_bits |= CONFIG_CHANGE_BIT_ASPECT_RATIO;
  if (old_post_processing_shader != g_ActiveConfig.sPostProcessingShader)
    changed_bits |= CONFIG_CHANGE_BIT_POST_PROCESSING_SHADER;
  if (old_hdr != g_ActiveConfig.bHDR)
    changed_bits |= CONFIG_CHANGE_BIT_HDR;

  // No changes?
  if (changed_bits == 0)
    return;

  float old_scale = g_framebuffer_manager->GetEFBScale();

  // Framebuffer changed?
  if (changed_bits & (CONFIG_CHANGE_BIT_MULTISAMPLES | CONFIG_CHANGE_BIT_STEREO_MODE |
                      CONFIG_CHANGE_BIT_TARGET_SIZE | CONFIG_CHANGE_BIT_HDR))
  {
    g_framebuffer_manager->RecreateEFBFramebuffer();
  }

  if (old_scale != g_framebuffer_manager->GetEFBScale())
  {
    auto& system = Core::System::GetInstance();
    auto& pixel_shader_manager = system.GetPixelShaderManager();
    pixel_shader_manager.Dirty();
  }

  // Reload shaders if host config has changed.
  if (changed_bits & (CONFIG_CHANGE_BIT_HOST_CONFIG | CONFIG_CHANGE_BIT_MULTISAMPLES))
  {
    OSD::AddMessage("Video config changed, reloading shaders.", OSD::Duration::NORMAL);
    g_gfx->WaitForGPUIdle();
    g_vertex_manager->InvalidatePipelineObject();
    g_vertex_manager->NotifyCustomShaderCacheOfHostChange(new_host_config);
    g_shader_cache->SetHostConfig(new_host_config);
    g_shader_cache->Reload();
    g_framebuffer_manager->RecompileShaders();
  }

  // Viewport and scissor rect have to be reset since they will be scaled differently.
  if (changed_bits & CONFIG_CHANGE_BIT_TARGET_SIZE)
  {
    BPFunctions::SetScissorAndViewport();
  }

  // Notify all listeners
  ConfigChangedEvent::Trigger(changed_bits);

  // TODO: Move everything else to the ConfigChanged event
}

static Common::EventHook s_check_config_event = AfterFrameEvent::Register(
    [](Core::System&) { CheckForConfigChanges(); }, "CheckForConfigChanges");
