// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// IMPORTANT: UI etc should modify g_Config. Graphics code should read g_ActiveConfig.
// The reason for this is to get rid of race conditions etc when the configuration
// changes in the middle of a frame. This is done by copying g_Config to g_ActiveConfig
// at the start of every frame. Noone should ever change members of g_ActiveConfig
// directly.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"
#include "VideoCommon/VideoCommon.h"

constexpr int EFB_SCALE_AUTO_INTEGRAL = 0;

enum class AspectMode : int
{
  Auto,           // ~4:3 or ~16:9 (auto detected)
  ForceWide,      // ~16:9
  ForceStandard,  // ~4:3
  Stretch,
  Custom,         // Forced relative custom AR
  CustomStretch,  // Forced absolute custom AR
  Raw,            // Forced squared pixels
};

enum class StereoMode : int
{
  Off,
  SBS,
  TAB,
  Anaglyph,
  QuadBuffer,
  Passive
};

enum class ShaderCompilationMode : int
{
  Synchronous,
  SynchronousUberShaders,
  AsynchronousUberShaders,
  AsynchronousSkipRendering
};

enum class TextureFilteringMode : int
{
  Default,
  Nearest,
  Linear,
};

enum class OutputResamplingMode : int
{
  Default,
  Bilinear,
  BSpline,
  MitchellNetravali,
  CatmullRom,
  SharpBilinear,
  AreaSampling,
};

enum class ColorCorrectionRegion : int
{
  SMPTE_NTSCM,
  SYSTEMJ_NTSCJ,
  EBU_PAL,
};

enum class TriState : int
{
  Off,
  On,
  Auto
};

enum class FrameDumpResolutionType : int
{
  // Window resolution (not including potential back buffer black borders)
  WindowResolution,
  // The aspect ratio corrected XFB resolution (XFB pixels might not have been square)
  XFBAspectRatioCorrectedResolution,
  // The raw unscaled XFB resolution (based on "internal resolution" scale)
  XFBRawResolution,
};

// Bitmask containing information about which configuration has changed for the backend.
enum ConfigChangeBits : u32
{
  CONFIG_CHANGE_BIT_HOST_CONFIG = (1 << 0),
  CONFIG_CHANGE_BIT_MULTISAMPLES = (1 << 1),
  CONFIG_CHANGE_BIT_STEREO_MODE = (1 << 2),
  CONFIG_CHANGE_BIT_TARGET_SIZE = (1 << 3),
  CONFIG_CHANGE_BIT_ANISOTROPY = (1 << 4),
  CONFIG_CHANGE_BIT_FORCE_TEXTURE_FILTERING = (1 << 5),
  CONFIG_CHANGE_BIT_VSYNC = (1 << 6),
  CONFIG_CHANGE_BIT_BBOX = (1 << 7),
  CONFIG_CHANGE_BIT_ASPECT_RATIO = (1 << 8),
  CONFIG_CHANGE_BIT_POST_PROCESSING_SHADER = (1 << 9),
  CONFIG_CHANGE_BIT_HDR = (1 << 10),
};

// NEVER inherit from this class.
struct VideoConfig final
{
  VideoConfig() = default;
  void Refresh();
  void VerifyValidity();

  // General
  bool bVSync = false;
  bool bVSyncActive = false;
  bool bWidescreenHack = false;
  AspectMode aspect_mode{};
  int custom_aspect_width = 1;
  int custom_aspect_height = 1;
  AspectMode suggested_aspect_mode{};
  u32 widescreen_heuristic_transition_threshold = 0;
  float widescreen_heuristic_aspect_ratio_slop = 0.f;
  float widescreen_heuristic_standard_ratio = 0.f;
  float widescreen_heuristic_widescreen_ratio = 0.f;
  bool bCrop = false;  // Aspect ratio controls.
  bool bShaderCache = false;

  // Enhancements
  u32 iMultisamples = 0;
  bool bSSAA = false;
  int iEFBScale = 0;
  TextureFilteringMode texture_filtering_mode = TextureFilteringMode::Default;
  OutputResamplingMode output_resampling_mode = OutputResamplingMode::Default;
  int iMaxAnisotropy = 0;
  std::string sPostProcessingShader;
  bool bForceTrueColor = false;
  bool bDisableCopyFilter = false;
  bool bArbitraryMipmapDetection = false;
  float fArbitraryMipmapDetectionThreshold = 0;
  bool bHDR = false;

  // Color Correction
  struct
  {
    // Color Space Correction:
    bool bCorrectColorSpace = false;
    ColorCorrectionRegion game_color_space = ColorCorrectionRegion::SMPTE_NTSCM;

    // Gamma Correction:
    bool bCorrectGamma = false;
    float fGameGamma = 2.35f;
    bool bSDRDisplayGammaSRGB = true;
    // Custom gamma when the display is not sRGB
    float fSDRDisplayCustomGamma = 2.2f;

    // HDR:
    // 203 is a good default value that matches the brightness of many SDR screens.
    // It's also the value recommended by the ITU.
    float fHDRPaperWhiteNits = 203.f;
  } color_correction;

  // Information
  bool bShowFPS = false;
  bool bShowFTimes = false;
  bool bShowVPS = false;
  bool bShowVTimes = false;
  bool bShowGraphs = false;
  bool bShowSpeed = false;
  bool bShowSpeedColors = false;
  int iPerfSampleUSec = 0;
  bool bShowNetPlayPing = false;
  bool bShowNetPlayMessages = false;
  bool bOverlayStats = false;
  bool bOverlayProjStats = false;
  bool bOverlayScissorStats = false;
  bool bTexFmtOverlayEnable = false;
  bool bTexFmtOverlayCenter = false;
  bool bLogRenderTimeToFile = false;

  // Render
  bool bWireFrame = false;
  bool bDisableFog = false;

  // Utility
  bool bDumpTextures = false;
  bool bDumpMipmapTextures = false;
  bool bDumpBaseTextures = false;
  bool bHiresTextures = false;
  bool bCacheHiresTextures = false;
  bool bDumpEFBTarget = false;
  bool bDumpXFBTarget = false;
  bool bDumpFramesAsImages = false;
  bool bUseFFV1 = false;
  std::string sDumpCodec;
  std::string sDumpPixelFormat;
  std::string sDumpEncoder;
  std::string sDumpFormat;
  std::string sDumpPath;
  FrameDumpResolutionType frame_dumps_resolution_type =
      FrameDumpResolutionType::XFBAspectRatioCorrectedResolution;
  bool bBorderlessFullscreen = false;
  bool bEnableGPUTextureDecoding = false;
  bool bPreferVSForLinePointExpansion = false;
  int iBitrateKbps = 0;
  bool bGraphicMods = false;
  std::optional<GraphicsModSystem::Config::GraphicsModGroup> graphics_mod_config;

  // Hacks
  bool bEFBAccessEnable = false;
  bool bEFBAccessDeferInvalidation = false;
  bool bPerfQueriesEnable = false;
  bool bBBoxEnable = false;
  bool bForceProgressive = false;
  bool bCPUCull = false;

  bool bEFBEmulateFormatChanges = false;
  bool bSkipEFBCopyToRam = false;
  bool bSkipXFBCopyToRam = false;
  bool bDisableCopyToVRAM = false;
  bool bDeferEFBCopies = false;
  bool bImmediateXFB = false;
  bool bSkipPresentingDuplicateXFBs = false;
  bool bCopyEFBScaled = false;
  int iSafeTextureCache_ColorSamples = 0;
  float fAspectRatioHackW = 1;  // Initial value needed for the first frame
  float fAspectRatioHackH = 1;
  bool bEnablePixelLighting = false;
  bool bFastDepthCalc = false;
  bool bVertexRounding = false;
  bool bVISkip = false;
  int iEFBAccessTileSize = 0;
  int iSaveTargetId = 0;  // TODO: Should be dropped
  u32 iMissingColorValue = 0;
  bool bFastTextureSampling = false;
#ifdef __APPLE__
  bool bNoMipmapping = false;  // Used by macOS fifoci to work around an M1 bug
#endif

  // Stereoscopy
  StereoMode stereo_mode{};
  int iStereoDepth = 0;
  int iStereoConvergence = 0;
  int iStereoConvergencePercentage = 0;
  bool bStereoSwapEyes = false;
  bool bStereoEFBMonoDepth = false;
  int iStereoDepthPercentage = 0;

  // D3D only config, mostly to be merged into the above
  int iAdapter = 0;

  // Metal only config
  TriState iManuallyUploadBuffers = TriState::Auto;
  TriState iUsePresentDrawable = TriState::Auto;

  // Enable API validation layers, currently only supported with Vulkan.
  bool bEnableValidationLayer = false;

  // Multithreaded submission, currently only supported with Vulkan.
  bool bBackendMultithreading = true;

  // Early command buffer execution interval in number of draws.
  // Currently only supported with Vulkan.
  int iCommandBufferExecuteInterval = 0;

  // Shader compilation settings.
  bool bWaitForShadersBeforeStarting = false;
  ShaderCompilationMode iShaderCompilationMode{};

  // Number of shader compiler threads.
  // 0 disables background compilation.
  // -1 uses an automatic number based on the CPU threads.
  int iShaderCompilerThreads = 0;
  int iShaderPrecompilerThreads = 0;

  // Loading custom drivers on Android
  std::string customDriverLibraryName;

  // Static config per API
  // TODO: Move this out of VideoConfig
  struct
  {
    APIType api_type = APIType::Nothing;
    std::string DisplayName;

    std::vector<std::string> Adapters;  // for D3D
    std::vector<u32> AAModes;

    // TODO: merge AdapterName and Adapters array
    std::string AdapterName;  // for OpenGL

    u32 MaxTextureSize = 16384;
    bool bUsesLowerLeftOrigin = false;
    bool bUsesExplictQuadBuffering = false;

    bool bSupportsExclusiveFullscreen = false;
    bool bSupportsDualSourceBlend = false;
    bool bSupportsPrimitiveRestart = false;
    bool bSupportsGeometryShaders = false;
    bool bSupportsComputeShaders = false;
    bool bSupports3DVision = false;
    bool bSupportsEarlyZ = false;         // needed by PixelShaderGen, so must stay in VideoCommon
    bool bSupportsBindingLayout = false;  // Needed by ShaderGen, so must stay in VideoCommon
    bool bSupportsBBox = false;
    bool bSupportsGSInstancing = false;  // Needed by GeometryShaderGen, so must stay in VideoCommon
    bool bSupportsPostProcessing = false;
    bool bSupportsPaletteConversion = false;
    bool bSupportsClipControl = false;  // Needed by VertexShaderGen, so must stay in VideoCommon
    bool bSupportsSSAA = false;
    bool bSupportsFragmentStoresAndAtomics = false;  // a.k.a. OpenGL SSBOs a.k.a. Direct3D UAVs
    bool bSupportsDepthClamp = false;  // Needed by VertexShaderGen, so must stay in VideoCommon
    bool bSupportsReversedDepthRange = false;
    bool bSupportsLogicOp = false;
    bool bSupportsMultithreading = false;
    bool bSupportsGPUTextureDecoding = false;
    bool bSupportsST3CTextures = false;
    bool bSupportsCopyToVram = false;
    bool bSupportsBitfield = false;  // Needed by UberShaders, so must stay in VideoCommon
    // Needed by UberShaders, so must stay in VideoCommon
    bool bSupportsDynamicSamplerIndexing = false;
    bool bSupportsBPTCTextures = false;
    bool bSupportsFramebufferFetch = false;  // Used as an alternative to dual-source blend on GLES
    bool bSupportsBackgroundCompiling = false;
    bool bSupportsLargePoints = false;
    bool bSupportsPartialDepthCopies = false;
    bool bSupportsDepthReadback = false;
    bool bSupportsShaderBinaries = false;
    bool bSupportsPipelineCacheData = false;
    bool bSupportsCoarseDerivatives = false;
    bool bSupportsTextureQueryLevels = false;
    bool bSupportsLodBiasInSampler = false;
    bool bSupportsSettingObjectNames = false;
    bool bSupportsPartialMultisampleResolve = false;
    bool bSupportsDynamicVertexLoader = false;
    bool bSupportsVSLinePointExpand = false;
    bool bSupportsGLLayerInFS = true;
    bool bSupportsHDROutput = false;
  } backend_info;

  // Utility
  bool UseVSForLinePointExpand() const
  {
    if (!backend_info.bSupportsVSLinePointExpand)
      return false;
    if (!backend_info.bSupportsGeometryShaders)
      return true;
    return bPreferVSForLinePointExpansion;
  }
  bool MultisamplingEnabled() const { return iMultisamples > 1; }
  bool ExclusiveFullscreenEnabled() const
  {
    return backend_info.bSupportsExclusiveFullscreen && !bBorderlessFullscreen;
  }
  bool UseGPUTextureDecoding() const
  {
    return backend_info.bSupportsGPUTextureDecoding && bEnableGPUTextureDecoding;
  }
  bool UseVertexRounding() const { return bVertexRounding && iEFBScale != 1; }
  bool ManualTextureSamplingWithCustomTextureSizes() const
  {
    // If manual texture sampling is disabled, we don't need to do anything.
    if (bFastTextureSampling)
      return false;
    // Hi-res textures break the wrapping logic used by manual texture sampling, as a texture's
    // size won't match the size the game sets.
    if (bHiresTextures)
      return true;
    // Hi-res EFB copies (but not native-resolution EFB copies at higher internal resolutions)
    // also result in different texture sizes that need special handling.
    if (iEFBScale != 1 && bCopyEFBScaled)
      return true;
    // Stereoscopic 3D changes the number of layers some textures have (EFB copies have 2 layers,
    // while game textures still have 1), meaning bounds checks need to be added.
    if (stereo_mode != StereoMode::Off)
      return true;
    // Otherwise, manual texture sampling can use the sizes games specify directly.
    return false;
  }
  bool UsingUberShaders() const;
  u32 GetShaderCompilerThreads() const;
  u32 GetShaderPrecompilerThreads() const;

  float GetCustomAspectRatio() const { return (float)custom_aspect_width / custom_aspect_height; }
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
void CheckForConfigChanges();
