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

// Log in two categories, and save three other options in the same byte
#define CONF_LOG 1
#define CONF_PRIMLOG 2
#define CONF_SAVETARGETS 8
#define CONF_SAVESHADERS 16

constexpr int EFB_SCALE_AUTO_INTEGRAL = 0;

enum class AspectMode : int
{
  Auto,
  AnalogWide,
  Analog,
  Stretch,
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
  AspectMode suggested_aspect_mode{};
  bool bCrop = false;  // Aspect ratio controls.
  bool bShaderCache = false;

  // Enhancements
  u32 iMultisamples = 0;
  bool bSSAA = false;
  int iEFBScale = 0;
  bool bForceFiltering = false;
  int iMaxAnisotropy = 0;
  std::string sPostProcessingShader;
  bool bForceTrueColor = false;
  bool bDisableCopyFilter = false;
  bool bArbitraryMipmapDetection = false;
  float fArbitraryMipmapDetectionThreshold = 0;

  // Information
  bool bShowFPS = false;
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
  bool bInternalResolutionFrameDumps = false;
  bool bBorderlessFullscreen = false;
  bool bEnableGPUTextureDecoding = false;
  int iBitrateKbps = 0;
  bool bGraphicMods = false;
  std::optional<GraphicsModGroupConfig> graphics_mod_config;

  // Hacks
  bool bEFBAccessEnable = false;
  bool bEFBAccessDeferInvalidation = false;
  bool bPerfQueriesEnable = false;
  bool bBBoxEnable = false;
  bool bForceProgressive = false;

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
  int iEFBAccessTileSize = 0;
  int iLog = 0;           // CONF_ bits
  int iSaveTargetId = 0;  // TODO: Should be dropped
  u32 iMissingColorValue = 0;
  bool bFastTextureSampling = false;

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

  // VideoSW Debugging
  int drawStart = 0;
  int drawEnd = 0;
  bool bDumpObjects = false;
  bool bDumpTevStages = false;
  bool bDumpTevTextureFetches = false;

  // Enable API validation layers, currently only supported with Vulkan.
  bool bEnableValidationLayer = false;

  // Multithreaded submission, currently only supported with Vulkan.
#if defined(ANDROID)
  bool bBackendMultithreading = false;
#else
  bool bBackendMultithreading = true;
#endif

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

  // Static config per API
  // TODO: Move this out of VideoConfig
  struct
  {
    APIType api_type = APIType::Nothing;

    std::vector<std::string> Adapters;  // for D3D
    std::vector<u32> AAModes;

    // TODO: merge AdapterName and Adapters array
    std::string AdapterName;  // for OpenGL

    u32 MaxTextureSize = 16384;
    bool bUsesLowerLeftOrigin = false;

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
  } backend_info;

  // Utility
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
  bool ManualTextureSamplingWithHiResTextures() const
  {
    // Hi-res textures (including hi-res EFB copies, but not native-resolution EFB copies at higher
    // internal resolutions) breaks the wrapping logic used by manual texture sampling.
    if (bFastTextureSampling)
      return false;
    if (iEFBScale != 1 && bCopyEFBScaled)
      return true;
    return bHiresTextures;
  }
  bool UsingUberShaders() const;
  u32 GetShaderCompilerThreads() const;
  u32 GetShaderPrecompilerThreads() const;
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
