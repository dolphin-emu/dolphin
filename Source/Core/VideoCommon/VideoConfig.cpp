// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VideoConfig.h"

#include <algorithm>
#include <optional>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Contains.h"

#include "Core/CPUThreadConfigCallback.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/FreeLookCamera.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/XFMemory.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;
BackendInfo g_backend_info;
static std::optional<CPUThreadConfigCallback::ConfigChangedCallbackID>
    s_config_changed_callback_id = std::nullopt;
static Common::EventHook s_check_config_event;

static bool IsVSyncActive(bool enabled)
{
  // Vsync is disabled when the throttler is disabled by the tab key.
  return enabled && !Core::GetIsThrottlerTempDisabled() &&
         Config::Get(Config::MAIN_EMULATION_SPEED) == 1.0;
}

void UpdateActiveConfig()
{
  g_ActiveConfig = g_Config;
  g_ActiveConfig.bVSyncActive = IsVSyncActive(g_ActiveConfig.bVSync);
}

void VideoConfig::Refresh()
{
  if (!s_config_changed_callback_id.has_value())
  {
    // There was a race condition between the video thread and the host thread here, if
    // corrections need to be made by VerifyValidity(). Briefly, the config will contain
    // invalid values. Instead, pause the video thread first, update the config and correct
    // it, then resume emulation, after which the video thread will detect the config has
    // changed and act accordingly.
    const auto config_changed_callback = [] {
      auto& system = Core::System::GetInstance();

      const bool lock_gpu_thread = Core::IsRunning(system);
      if (lock_gpu_thread)
        system.GetFifo().PauseAndLock();

      g_Config.Refresh();
      g_Config.VerifyValidity();

      if (lock_gpu_thread)
        system.GetFifo().RestoreState(true);
    };

    s_config_changed_callback_id =
        CPUThreadConfigCallback::AddConfigChangedCallback(config_changed_callback);
  }

  bVSync = Config::Get(Config::GFX_VSYNC);
  iAdapter = Config::Get(Config::GFX_ADAPTER);
  iManuallyUploadBuffers = Config::Get(Config::GFX_MTL_MANUALLY_UPLOAD_BUFFERS);
  iUsePresentDrawable = Config::Get(Config::GFX_MTL_USE_PRESENT_DRAWABLE);

  bWidescreenHack = Config::Get(Config::GFX_WIDESCREEN_HACK);
  aspect_mode = Config::Get(Config::GFX_ASPECT_RATIO);
  custom_aspect_width = Config::Get(Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH);
  custom_aspect_height = Config::Get(Config::GFX_CUSTOM_ASPECT_RATIO_HEIGHT);
  suggested_aspect_mode = Config::Get(Config::GFX_SUGGESTED_ASPECT_RATIO);
  widescreen_heuristic_transition_threshold =
      Config::Get(Config::GFX_WIDESCREEN_HEURISTIC_TRANSITION_THRESHOLD);
  widescreen_heuristic_aspect_ratio_slop =
      Config::Get(Config::GFX_WIDESCREEN_HEURISTIC_ASPECT_RATIO_SLOP);
  widescreen_heuristic_standard_ratio =
      Config::Get(Config::GFX_WIDESCREEN_HEURISTIC_STANDARD_RATIO);
  widescreen_heuristic_widescreen_ratio =
      Config::Get(Config::GFX_WIDESCREEN_HEURISTIC_WIDESCREEN_RATIO);
  bCrop = Config::Get(Config::GFX_CROP);
  iSafeTextureCache_ColorSamples = Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);
  bShowFPS = Config::Get(Config::GFX_SHOW_FPS);
  bShowFTimes = Config::Get(Config::GFX_SHOW_FTIMES);
  bShowVPS = Config::Get(Config::GFX_SHOW_VPS);
  bShowVTimes = Config::Get(Config::GFX_SHOW_VTIMES);
  bShowGraphs = Config::Get(Config::GFX_SHOW_GRAPHS);
  bShowSpeed = Config::Get(Config::GFX_SHOW_SPEED);
  bShowSpeedColors = Config::Get(Config::GFX_SHOW_SPEED_COLORS);
  iPerfSampleUSec = Config::Get(Config::GFX_PERF_SAMP_WINDOW) * 1000;
  bLogRenderTimeToFile = Config::Get(Config::GFX_LOG_RENDER_TIME_TO_FILE);
  bOverlayStats = Config::Get(Config::GFX_OVERLAY_STATS);
  bOverlayProjStats = Config::Get(Config::GFX_OVERLAY_PROJ_STATS);
  bOverlayScissorStats = Config::Get(Config::GFX_OVERLAY_SCISSOR_STATS);
  bDumpTextures = Config::Get(Config::GFX_DUMP_TEXTURES);
  bDumpMipmapTextures = Config::Get(Config::GFX_DUMP_MIP_TEXTURES);
  bDumpBaseTextures = Config::Get(Config::GFX_DUMP_BASE_TEXTURES);
  bHiresTextures = Config::Get(Config::GFX_HIRES_TEXTURES);
  bCacheHiresTextures = Config::Get(Config::GFX_CACHE_HIRES_TEXTURES);
  bDumpEFBTarget = Config::Get(Config::GFX_DUMP_EFB_TARGET);
  bDumpXFBTarget = Config::Get(Config::GFX_DUMP_XFB_TARGET);
  bEnableGPUTextureDecoding = Config::Get(Config::GFX_ENABLE_GPU_TEXTURE_DECODING);
  bPreferVSForLinePointExpansion = Config::Get(Config::GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION);
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
  bWaitForShadersBeforeStarting = Config::Get(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING);
  iShaderCompilationMode = Config::Get(Config::GFX_SHADER_COMPILATION_MODE);
  iShaderCompilerThreads = Config::Get(Config::GFX_SHADER_COMPILER_THREADS);
  iShaderPrecompilerThreads = Config::Get(Config::GFX_SHADER_PRECOMPILER_THREADS);
  bCPUCull = Config::Get(Config::GFX_CPU_CULL);

  texture_filtering_mode = Config::Get(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING);
  iMaxAnisotropy = Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY);
  output_resampling_mode = Config::Get(Config::GFX_ENHANCE_OUTPUT_RESAMPLING);
  sPostProcessingShader = Config::Get(Config::GFX_ENHANCE_POST_SHADER);
  bForceTrueColor = Config::Get(Config::GFX_ENHANCE_FORCE_TRUE_COLOR);
  bDisableCopyFilter = Config::Get(Config::GFX_ENHANCE_DISABLE_COPY_FILTER);
  bArbitraryMipmapDetection = Config::Get(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION);
  fArbitraryMipmapDetectionThreshold =
      Config::Get(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD);
  bHDR = Config::Get(Config::GFX_ENHANCE_HDR_OUTPUT);

  color_correction.bCorrectColorSpace = Config::Get(Config::GFX_CC_CORRECT_COLOR_SPACE);
  color_correction.game_color_space = Config::Get(Config::GFX_CC_GAME_COLOR_SPACE);
  color_correction.bCorrectGamma = Config::Get(Config::GFX_CC_CORRECT_GAMMA);
  color_correction.fGameGamma = Config::Get(Config::GFX_CC_GAME_GAMMA);
  color_correction.bSDRDisplayGammaSRGB = Config::Get(Config::GFX_CC_SDR_DISPLAY_GAMMA_SRGB);
  color_correction.fSDRDisplayCustomGamma = Config::Get(Config::GFX_CC_SDR_DISPLAY_CUSTOM_GAMMA);
  color_correction.fHDRPaperWhiteNits = Config::Get(Config::GFX_CC_HDR_PAPER_WHITE_NITS);

  stereo_mode = Config::Get(Config::GFX_STEREO_MODE);
  stereo_per_eye_resolution_full = Config::Get(Config::GFX_STEREO_PER_EYE_RESOLUTION_FULL);
  stereo_depth = Config::Get(Config::GFX_STEREO_DEPTH) *
                 Config::Get(Config::GFX_STEREO_DEPTH_PERCENTAGE) * 0.00001f;
  stereo_convergence = Config::Get(Config::GFX_STEREO_CONVERGENCE) *
                       Config::Get(Config::GFX_STEREO_CONVERGENCE_PERCENTAGE) * 0.01f;
  bStereoSwapEyes = Config::Get(Config::GFX_STEREO_SWAP_EYES);
  bStereoEFBMonoDepth = Config::Get(Config::GFX_STEREO_EFB_MONO_DEPTH);

  bEFBAccessEnable = Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
  bEFBAccessDeferInvalidation = Config::Get(Config::GFX_HACK_EFB_DEFER_INVALIDATION);
  bBBoxEnable = Config::Get(Config::GFX_HACK_BBOX_ENABLE);
  bSkipEFBCopyToRam = Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
  bSkipXFBCopyToRam = Config::Get(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM);
  bDisableCopyToVRAM = Config::Get(Config::GFX_HACK_DISABLE_COPY_TO_VRAM);
  bDeferEFBCopies = Config::Get(Config::GFX_HACK_DEFER_EFB_COPIES);
  bImmediateXFB = Config::Get(Config::GFX_HACK_IMMEDIATE_XFB);
  bVISkip = Config::Get(Config::GFX_HACK_VI_SKIP);
  bSkipPresentingDuplicateXFBs = bVISkip || Config::Get(Config::GFX_HACK_SKIP_DUPLICATE_XFBS);
  bCopyEFBScaled = Config::Get(Config::GFX_HACK_COPY_EFB_SCALED);
  bEFBEmulateFormatChanges = Config::Get(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES);
  bVertexRounding = Config::Get(Config::GFX_HACK_VERTEX_ROUNDING);
  iEFBAccessTileSize = Config::Get(Config::GFX_HACK_EFB_ACCESS_TILE_SIZE);
  iMissingColorValue = Config::Get(Config::GFX_HACK_MISSING_COLOR_VALUE);
  bFastTextureSampling = Config::Get(Config::GFX_HACK_FAST_TEXTURE_SAMPLING);
#ifdef __APPLE__
  bNoMipmapping = Config::Get(Config::GFX_HACK_NO_MIPMAPPING);
#endif

  bPerfQueriesEnable = Config::Get(Config::GFX_PERF_QUERIES_ENABLE);

  bGraphicMods = Config::Get(Config::GFX_MODS_ENABLE);

  customDriverLibraryName = Config::Get(Config::GFX_DRIVER_LIB_NAME);

  vertex_loader_type = Config::Get(Config::GFX_VERTEX_LOADER_TYPE);
}

void VideoConfig::VerifyValidity()
{
  // TODO: Check iMaxAnisotropy value
  if (iAdapter < 0 || iAdapter > ((int)g_backend_info.Adapters.size() - 1))
    iAdapter = 0;

  if (!Common::Contains(g_backend_info.AAModes, iMultisamples))
    iMultisamples = 1;

  if (stereo_mode != StereoMode::Off)
  {
    if (!g_backend_info.bSupportsGeometryShaders && !g_backend_info.bSupportsVSLayerOutput)
    {
      OSD::AddMessage("Stereoscopic 3D isn't supported by your GPU, support for geometry shaders "
                      "or VS layer output is required.",
                      10000);
      stereo_mode = StereoMode::Off;
    }
  }
}

void VideoConfig::Init()
{
  s_check_config_event =
      GetVideoEvents().after_frame_event.Register([](Core::System&) { CheckForConfigChanges(); });
}

void VideoConfig::Shutdown()
{
  s_check_config_event.reset();

  if (!s_config_changed_callback_id.has_value())
    return;

  CPUThreadConfigCallback::RemoveConfigChangedCallback(*s_config_changed_callback_id);
  s_config_changed_callback_id.reset();
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
  if (!g_backend_info.bSupportsBackgroundCompiling)
    return 0;

  if (iShaderCompilerThreads >= 0)
    return static_cast<u32>(iShaderCompilerThreads);
  else
    return GetNumAutoShaderCompilerThreads();
}

u32 VideoConfig::GetShaderPrecompilerThreads() const
{
  // When using background compilation, always keep the same thread count.
  if (!bWaitForShadersBeforeStarting)
    return GetShaderCompilerThreads();

  if (!g_backend_info.bSupportsBackgroundCompiling)
    return 0;

  if (iShaderPrecompilerThreads >= 0)
    return static_cast<u32>(iShaderPrecompilerThreads);
  else if (!DriverDetails::HasBug(DriverDetails::BUG_BROKEN_MULTITHREADED_SHADER_PRECOMPILATION))
    return GetNumAutoShaderPreCompilerThreads();
  else
    return 1;
}

void CheckForConfigChanges()
{
  const ShaderHostConfig old_shader_host_config = ShaderHostConfig::GetCurrent();
  const StereoMode old_stereo = g_ActiveConfig.stereo_mode;
  const float old_stereo_depth = g_ActiveConfig.stereo_depth;
  const float old_stereo_convergence = g_ActiveConfig.stereo_convergence;
  const bool old_stereo_swap_eyes = g_ActiveConfig.bStereoSwapEyes;
  const u32 old_multisamples = g_ActiveConfig.iMultisamples;
  const auto old_anisotropy = g_ActiveConfig.iMaxAnisotropy;
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
  g_vertex_manager->OnConfigChange();

  g_freelook_camera.RefreshConfig();

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

  // Check if stereo settings changed (depth, convergence, swap eyes)
  // If so, we need to trigger SetProjectionChanged to recalculate stereoparams
  const bool stereo_settings_changed =
      (old_stereo != g_ActiveConfig.stereo_mode) ||
      (old_stereo_depth != g_ActiveConfig.stereo_depth) ||
      (old_stereo_convergence != g_ActiveConfig.stereo_convergence) ||
      (old_stereo_swap_eyes != g_ActiveConfig.bStereoSwapEyes);

  if (stereo_settings_changed && g_ActiveConfig.stereo_mode != StereoMode::Off)
  {
    auto& system = Core::System::GetInstance();
    system.GetGeometryShaderManager().SetProjectionChanged();
  }

  // No changes?
  if (changed_bits == 0 && !stereo_settings_changed)
    return;

  float old_scale = g_framebuffer_manager->GetEFBScale();

  // Framebuffer changed?
  if (changed_bits & (CONFIG_CHANGE_BIT_MULTISAMPLES | CONFIG_CHANGE_BIT_STEREO_MODE |
                      CONFIG_CHANGE_BIT_TARGET_SIZE | CONFIG_CHANGE_BIT_HDR))
  {
    g_framebuffer_manager->RecreateEFBFramebuffer(g_ActiveConfig.iEFBScale);
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
    BPFunctions::SetScissorAndViewport(g_framebuffer_manager.get(), bpmem.scissorTL,
                                       bpmem.scissorBR, bpmem.scissorOffset, xfmem.viewport);
  }

  // Notify all listeners
  GetVideoEvents().config_changed_event.Trigger(changed_bits);

  // TODO: Move everything else to the ConfigChanged event
}
