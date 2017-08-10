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
#include "VideoCommon/VideoCommon.h"

// Log in two categories, and save three other options in the same byte
#define CONF_LOG 1
#define CONF_PRIMLOG 2
#define CONF_SAVETARGETS 8
#define CONF_SAVESHADERS 16

constexpr int EFB_SCALE_AUTO_INTEGRAL = 0;

enum AspectMode
{
  ASPECT_AUTO = 0,
  ASPECT_ANALOG_WIDE = 1,
  ASPECT_ANALOG = 2,
  ASPECT_STRETCH = 3,
};

enum StereoMode
{
  STEREO_OFF = 0,
  STEREO_SBS,
  STEREO_TAB,
  STEREO_ANAGLYPH,
  STEREO_QUADBUFFER,
  STEREO_3DVISION
};

struct ProjectionHackConfig final
{
  bool m_enable;
  bool m_sznear;
  bool m_szfar;
  std::string m_znear;
  std::string m_zfar;
};

// NEVER inherit from this class.
struct VideoConfig final
{
  VideoConfig();
  void Refresh();
  void VerifyValidity();
  void UpdateProjectionHack();
  bool IsVSync();

  // General
  bool bVSync;
  bool bWidescreenHack;
  int iAspectRatio;
  bool bCrop;  // Aspect ratio controls.
  bool bUseXFB;
  bool bUseRealXFB;
  bool bShaderCache;

  // Enhancements
  u32 iMultisamples;
  bool bSSAA;
  int iEFBScale;
  bool bForceFiltering;
  int iMaxAnisotropy;
  std::string sPostProcessingShader;
  bool bForceTrueColor;

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
  bool bConvertHiresTextures;
  bool bCacheHiresTextures;
  bool bDumpEFBTarget;
  bool bDumpFramesAsImages;
  bool bUseFFV1;
  std::string sDumpCodec;
  std::string sDumpFormat;
  std::string sDumpPath;
  bool bInternalResolutionFrameDumps;
  bool bFreeLook;
  bool bBorderlessFullscreen;
  bool bEnableGPUTextureDecoding;
  int iBitrateKbps;

  // Hacks
  bool bEFBAccessEnable;
  bool bPerfQueriesEnable;
  bool bBBoxEnable;
  bool bBBoxPreferStencilImplementation;  // OpenGL-only, to see how slow it is compared to SSBOs
  bool bForceProgressive;

  bool bEFBEmulateFormatChanges;
  bool bSkipEFBCopyToRam;
  bool bCopyEFBScaled;
  int iSafeTextureCache_ColorSamples;
  ProjectionHackConfig phack;
  float fAspectRatioHackW, fAspectRatioHackH;
  bool bEnablePixelLighting;
  bool bFastDepthCalc;
  bool bVertexRounding;
  int iLog;           // CONF_ bits
  int iSaveTargetId;  // TODO: Should be dropped

  // Stereoscopy
  int iStereoMode;
  int iStereoDepth;
  int iStereoConvergence;
  int iStereoConvergencePercentage;
  bool bStereoSwapEyes;
  bool bStereoEFBMonoDepth;
  int iStereoDepthPercentage;

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

  // The following options determine the ubershader mode:
  //   No ubershaders:
  //     - bBackgroundShaderCompiling = false
  //     - bDisableSpecializedShaders = false
  //   Hybrid/background compiling:
  //     - bBackgroundShaderCompiling = true
  //     - bDisableSpecializedShaders = false
  //   Ubershaders only:
  //     - bBackgroundShaderCompiling = false
  //     - bDisableSpecializedShaders = true

  // Enable background shader compiling, use ubershaders while waiting.
  bool bBackgroundShaderCompiling;

  // Use ubershaders only, don't compile specialized shaders.
  bool bDisableSpecializedShaders;

  // Precompile ubershader variants at boot/config reload time.
  bool bPrecompileUberShaders;

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

    bool bSupportsExclusiveFullscreen;
    bool bSupportsDualSourceBlend;
    bool bSupportsPrimitiveRestart;
    bool bSupportsOversizedViewports;
    bool bSupportsGeometryShaders;
    bool bSupportsComputeShaders;
    bool bSupports3DVision;
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
    bool bSupportsMultithreading;
    bool bSupportsInternalResolutionFrameDumps;
    bool bSupportsGPUTextureDecoding;
    bool bSupportsST3CTextures;
    bool bSupportsBitfield;                // Needed by UberShaders, so must stay in VideoCommon
    bool bSupportsDynamicSamplerIndexing;  // Needed by UberShaders, so must stay in VideoCommon
    bool bSupportsBPTCTextures;
  } backend_info;

  // Utility
  bool RealXFBEnabled() const { return bUseXFB && bUseRealXFB; }
  bool VirtualXFBEnabled() const { return bUseXFB && !bUseRealXFB; }
  bool ExclusiveFullscreenEnabled() const
  {
    return backend_info.bSupportsExclusiveFullscreen && !bBorderlessFullscreen;
  }
  bool BBoxUseFragmentShaderImplementation() const
  {
    if (backend_info.api_type == APIType::OpenGL && bBBoxPreferStencilImplementation)
      return false;
    return backend_info.bSupportsBBox && backend_info.bSupportsFragmentStoresAndAtomics;
  }
  bool UseGPUTextureDecoding() const
  {
    return backend_info.bSupportsGPUTextureDecoding && bEnableGPUTextureDecoding;
  }
  bool UseVertexRounding() const { return bVertexRounding && iEFBScale != 1; }
  u32 GetShaderCompilerThreads() const;
  u32 GetShaderPrecompilerThreads() const;
  bool CanPrecompileUberShaders() const;
  bool CanBackgroundCompileShaders() const;
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
