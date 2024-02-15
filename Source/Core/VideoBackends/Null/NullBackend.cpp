// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Null Backend Documentation

// This backend tries not to do anything in the backend,
// but everything in VideoCommon.

#include "VideoBackends/Null/VideoBackend.h"

#include "Common/Common.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Null/NullBoundingBox.h"
#include "VideoBackends/Null/NullGfx.h"
#include "VideoBackends/Null/NullVertexManager.h"
#include "VideoBackends/Null/PerfQuery.h"
#include "VideoBackends/Null/TextureCache.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace Null
{
void VideoBackend::InitBackendInfo(const WindowSystemInfo& wsi)
{
  g_Config.backend_info.api_type = APIType::Nothing;
  g_Config.backend_info.MaxTextureSize = 16384;
  g_Config.backend_info.bSupportsExclusiveFullscreen = true;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = true;
  g_Config.backend_info.bSupportsGeometryShaders = true;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupports3DVision = false;
  g_Config.backend_info.bSupportsEarlyZ = true;
  g_Config.backend_info.bSupportsBindingLayout = true;
  g_Config.backend_info.bSupportsBBox = true;
  g_Config.backend_info.bSupportsGSInstancing = true;
  g_Config.backend_info.bSupportsPostProcessing = false;
  g_Config.backend_info.bSupportsPaletteConversion = true;
  g_Config.backend_info.bSupportsClipControl = true;
  g_Config.backend_info.bSupportsSSAA = true;
  g_Config.backend_info.bSupportsDepthClamp = true;
  g_Config.backend_info.bSupportsReversedDepthRange = true;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsGPUTextureDecoding = false;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;
  g_Config.backend_info.bSupportsFramebufferFetch = false;
  g_Config.backend_info.bSupportsBackgroundCompiling = false;
  g_Config.backend_info.bSupportsLogicOp = false;
  g_Config.backend_info.bSupportsLargePoints = false;
  g_Config.backend_info.bSupportsDepthReadback = false;
  g_Config.backend_info.bSupportsPartialDepthCopies = false;
  g_Config.backend_info.bSupportsShaderBinaries = false;
  g_Config.backend_info.bSupportsPipelineCacheData = false;
  g_Config.backend_info.bSupportsCoarseDerivatives = false;
  g_Config.backend_info.bSupportsTextureQueryLevels = false;
  g_Config.backend_info.bSupportsLodBiasInSampler = false;
  g_Config.backend_info.bSupportsSettingObjectNames = false;
  g_Config.backend_info.bSupportsPartialMultisampleResolve = true;
  g_Config.backend_info.bSupportsDynamicVertexLoader = false;

  // aamodes: We only support 1 sample, so no MSAA
  g_Config.backend_info.Adapters.clear();
  g_Config.backend_info.AAModes = {1};
}

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  return InitializeShared(std::make_unique<NullGfx>(), std::make_unique<VertexManager>(),
                          std::make_unique<PerfQuery>(), std::make_unique<NullBoundingBox>(),
                          std::make_unique<NullRenderer>(), std::make_unique<TextureCache>());
}

void VideoBackend::Shutdown()
{
  ShutdownShared();
}

std::string VideoBackend::GetDisplayName() const
{
  // i18n: Null is referring to the null video backend, which renders nothing
  return _trans("Null");
}
}  // namespace Null
