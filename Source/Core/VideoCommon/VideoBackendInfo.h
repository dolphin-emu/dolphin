// Copyright 202 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/VideoCommon.h"

#include <string>
#include <vector>

// Static config per API
struct BackendInfo
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
};
