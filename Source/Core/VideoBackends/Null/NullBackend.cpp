// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Null Backend Documentation

// This backend tries not to do anything in the backend,
// but everything in VideoCommon.

#include "VideoBackends/Null/VideoBackend.h"

#include "Common/Common.h"

#include "VideoBackends/Null/NullBoundingBox.h"
#include "VideoBackends/Null/NullGfx.h"
#include "VideoBackends/Null/NullVertexManager.h"
#include "VideoBackends/Null/PerfQuery.h"
#include "VideoBackends/Null/TextureCache.h"

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace Null
{
void VideoBackend::InitBackendInfo(const WindowSystemInfo& wsi)
{
  g_backend_info.api_type = APIType::Nothing;
  g_backend_info.MaxTextureSize = 16384;
  g_backend_info.bSupportsExclusiveFullscreen = true;
  g_backend_info.bSupportsDualSourceBlend = true;
  g_backend_info.bSupportsPrimitiveRestart = true;
  g_backend_info.bSupportsGeometryShaders = true;
  g_backend_info.bSupportsComputeShaders = false;
  g_backend_info.bSupports3DVision = false;
  g_backend_info.bSupportsEarlyZ = true;
  g_backend_info.bSupportsBindingLayout = true;
  g_backend_info.bSupportsBBox = true;
  g_backend_info.bSupportsGSInstancing = true;
  g_backend_info.bSupportsPostProcessing = false;
  g_backend_info.bSupportsPaletteConversion = true;
  g_backend_info.bSupportsClipControl = true;
  g_backend_info.bSupportsSSAA = true;
  g_backend_info.bSupportsDepthClamp = true;
  g_backend_info.bSupportsReversedDepthRange = true;
  g_backend_info.bSupportsMultithreading = false;
  g_backend_info.bSupportsGPUTextureDecoding = false;
  g_backend_info.bSupportsST3CTextures = false;
  g_backend_info.bSupportsBPTCTextures = false;
  g_backend_info.bSupportsFramebufferFetch = false;
  g_backend_info.bSupportsBackgroundCompiling = false;
  g_backend_info.bSupportsLogicOp = false;
  g_backend_info.bSupportsLargePoints = false;
  g_backend_info.bSupportsDepthReadback = false;
  g_backend_info.bSupportsPartialDepthCopies = false;
  g_backend_info.bSupportsShaderBinaries = false;
  g_backend_info.bSupportsPipelineCacheData = false;
  g_backend_info.bSupportsCoarseDerivatives = false;
  g_backend_info.bSupportsTextureQueryLevels = false;
  g_backend_info.bSupportsLodBiasInSampler = false;
  g_backend_info.bSupportsSettingObjectNames = false;
  g_backend_info.bSupportsPartialMultisampleResolve = true;
  g_backend_info.bSupportsDynamicVertexLoader = false;

  // aamodes: We only support 1 sample, so no MSAA
  g_backend_info.Adapters.clear();
  g_backend_info.AAModes = {1};
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
