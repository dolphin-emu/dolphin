// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// IMPORTANT: UI etc should modify g_Config. Graphics code should read g_ActiveConfig.
// The reason for this is to get rid of race conditions etc when the configuration
// changes in the middle of a frame. This is done by copying g_Config to g_ActiveConfig
// at the start of every frame. Noone should ever change members of g_ActiveConfig
// directly.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

enum class APIType;

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

enum class ShaderCompilationMode : int
{
  Synchronous,
  AsynchronousSkipRendering
};

// NEVER inherit from this class.
struct VideoConfig final
{
  VideoConfig();
  void Refresh();
  void VerifyValidity();

  // Check in Render SwapImpl
  bool bDirty;

  // General
  bool bVSync;
  bool bVSyncActive;
  bool bWidescreenHack;
  AspectMode aspect_mode;
  AspectMode suggested_aspect_mode;
  float fDisplayScale;
  bool bCrop;  // Aspect ratio controls.
  bool bShaderCache;

  // Enhancements
  u32 iMultisamples;
  bool bSSAA;
  int iEFBScale;
  bool bForceFiltering;
  int iMaxAnisotropy;
  std::string sPostProcessingShader;
  bool bForceTrueColor;
  bool bDisableCopyFilter;
  bool bArbitraryMipmapDetection;
  float fArbitraryMipmapDetectionThreshold;

  // Information
  bool bShowFPS;
  bool bShowNetPlayPing;
  bool bShowNetPlayMessages;
  bool bOverlayStats;
  bool bOverlayProjStats;
  bool bTexFmtOverlayEnable;
  bool bTexFmtOverlayCenter;
  bool bLogRenderTimeToFile;

  // Render
  bool bWireFrame;
  bool bDisableFog;

  // Utility
  bool bDumpTextures;
  bool bHiresTextures;
  bool bCacheHiresTextures;
  bool bDumpEFBTarget;
  bool bDumpXFBTarget;
  bool bDumpFramesAsImages;
  bool bUseFFV1;
  std::string sDumpCodec;
  std::string sDumpEncoder;
  std::string sDumpFormat;
  std::string sDumpPath;
  bool bInternalResolutionFrameDumps;
  bool bFreeLook;
  bool bBorderlessFullscreen;
  bool bEnableGPUTextureDecoding;
  int iBitrateKbps;

  // Hacks
  bool bEFBAccessEnable;
  bool bEFBAccessDeferInvalidation;
  bool bPerfQueriesEnable;
  bool bBBoxEnable;
  bool bForceProgressive;

  bool bAlphaPassShadowHack; // alpha pass hack
  bool bLogicOpsDrawHack; // logic ops hack

  bool bEFBEmulateFormatChanges;
  bool bTMEMCacheEmulation;
  bool bSkipEFBCopyToRam;
  bool bSkipXFBCopyToRam;
  bool bDisableCopyToVRAM;
  bool bDeferEFBCopies;
  bool bImmediateXFB;
  bool bCopyEFBScaled;
  int iSafeTextureCache_ColorSamples;
  float fAspectRatioHackW, fAspectRatioHackH;
  bool bEnablePixelLighting;
  bool bFastDepthCalc;
  bool bVertexRounding;
  int iEFBAccessTileSize;
  int iLog;           // CONF_ bits
  int iSaveTargetId;  // TODO: Should be dropped

  // D3D only config, mostly to be merged into the above
  int iAdapter;

  // VideoSW Debugging
  int drawStart;
  int drawEnd;
  bool bZComploc;
  bool bZFreeze;
  bool bDumpObjects;
  bool bDumpTevStages;
  bool bDumpTevTextureFetches;

  // Enable API validation layers, currently only supported with Vulkan.
  bool bEnableValidationLayer;

  // Multithreaded submission, currently only supported with Vulkan.
  bool bBackendMultithreading;

  // Early command buffer execution interval in number of draws.
  // Currently only supported with Vulkan.
  int iCommandBufferExecuteInterval;

  // Shader compilation settings.
  bool bWaitForShadersBeforeStarting;
  ShaderCompilationMode iShaderCompilationMode;

  // Number of shader compiler threads.
  // 0 disables background compilation.
  // -1 uses an automatic number based on the CPU threads.
  int iShaderCompilerThreads;
  int iShaderPrecompilerThreads;

  // Static config per API
  // TODO: Move this out of VideoConfig
  struct
  {
    APIType api_type;

    std::vector<std::string> Adapters;  // for D3D
    std::vector<u32> AAModes;

    // TODO: merge AdapterName and Adapters array
    std::string AdapterName;  // for OpenGL

    u32 MaxTextureSize;
    bool bUsesLowerLeftOrigin;

    bool bSupportsExclusiveFullscreen;
    bool bSupportsDualSourceBlend;
    bool bSupportsOversizedViewports;
    bool bSupportsGeometryShaders;
    bool bSupportsComputeShaders;
    bool bSupportsEarlyZ;         // needed by PixelShaderGen, so must stay in VideoCommon
    bool bSupportsBindingLayout;  // Needed by ShaderGen, so must stay in VideoCommon
    bool bSupportsBBox;
    bool bSupportsGSInstancing;  // Needed by GeometryShaderGen, so must stay in VideoCommon
    bool bSupportsPostProcessing;
    bool bSupportsPaletteConversion;
    bool bSupportsClipControl;  // Needed by VertexShaderGen, so must stay in VideoCommon
    bool bSupportsSSAA;
    bool bSupportsFragmentStoresAndAtomics;  // a.k.a. OpenGL SSBOs a.k.a. Direct3D UAVs
    bool bSupportsDepthClamp;  // Needed by VertexShaderGen, so must stay in VideoCommon
    bool bSupportsReversedDepthRange;
    bool bSupportsLogicOp;
    bool bSupportsMultithreading;
    bool bSupportsGPUTextureDecoding;
    bool bSupportsST3CTextures;
    bool bSupportsCopyToVram;
    bool bSupportsBitfield;                // Needed by UberShaders, so must stay in VideoCommon
    bool bSupportsDynamicSamplerIndexing;  // Needed by UberShaders, so must stay in VideoCommon
    bool bSupportsBPTCTextures;
    bool bSupportsFramebufferFetch;  // Used as an alternative to dual-source blend on GLES
    bool bSupportsBackgroundCompiling;
    bool bSupportsLargePoints;
    bool bSupportsPartialDepthCopies;
    bool bSupportsShaderBinaries;
    bool bSupportsPipelineCacheData;
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
  u32 GetShaderCompilerThreads() const;
  u32 GetShaderPrecompilerThreads() const;
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
