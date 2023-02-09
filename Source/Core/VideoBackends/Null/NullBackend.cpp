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

class AbstractGfx;

namespace Null
{
void VideoBackend::InitBackendInfo()
{
  backend_info.api_type = APIType::Nothing;
  backend_info.MaxTextureSize = 16384;
  backend_info.bSupportsExclusiveFullscreen = true;
  backend_info.bSupportsDualSourceBlend = true;
  backend_info.bSupportsPrimitiveRestart = true;
  backend_info.bSupportsGeometryShaders = true;
  backend_info.bSupportsComputeShaders = false;
  backend_info.bSupports3DVision = false;
  backend_info.bSupportsEarlyZ = true;
  backend_info.bSupportsBindingLayout = true;
  backend_info.bSupportsBBox = true;
  backend_info.bSupportsGSInstancing = true;
  backend_info.bSupportsPostProcessing = false;
  backend_info.bSupportsPaletteConversion = true;
  backend_info.bSupportsClipControl = true;
  backend_info.bSupportsSSAA = true;
  backend_info.bSupportsDepthClamp = true;
  backend_info.bSupportsReversedDepthRange = true;
  backend_info.bSupportsMultithreading = false;
  backend_info.bSupportsGPUTextureDecoding = false;
  backend_info.bSupportsST3CTextures = false;
  backend_info.bSupportsBPTCTextures = false;
  backend_info.bSupportsFramebufferFetch = false;
  backend_info.bSupportsBackgroundCompiling = false;
  backend_info.bSupportsLogicOp = false;
  backend_info.bSupportsLargePoints = false;
  backend_info.bSupportsDepthReadback = false;
  backend_info.bSupportsPartialDepthCopies = false;
  backend_info.bSupportsShaderBinaries = false;
  backend_info.bSupportsPipelineCacheData = false;
  backend_info.bSupportsCoarseDerivatives = false;
  backend_info.bSupportsTextureQueryLevels = false;
  backend_info.bSupportsLodBiasInSampler = false;
  backend_info.bSupportsSettingObjectNames = false;
  backend_info.bSupportsPartialMultisampleResolve = true;
  backend_info.bSupportsDynamicVertexLoader = false;

  // aamodes: We only support 1 sample, so no MSAA
  backend_info.Adapters.clear();
  backend_info.AAModes = {1};
}

std::unique_ptr<AbstractGfx> VideoBackend::CreateGfx()
{
  return std::make_unique<NullGfx>(this);
}

std::unique_ptr<VertexManagerBase> VideoBackend::CreateVertexManager(AbstractGfx* gfx)
{
  return std::make_unique<VertexManager>();
}

std::unique_ptr<PerfQueryBase> VideoBackend::CreatePerfQuery(AbstractGfx* gfx)
{
  return std::make_unique<PerfQuery>();
}

std::unique_ptr<BoundingBox> VideoBackend::CreateBoundingBox(AbstractGfx* gfx)
{
  return std::make_unique<NullBoundingBox>();
}

std::unique_ptr<Renderer> VideoBackend::CreateRenderer(AbstractGfx* gfx)
{
  return std::make_unique<Renderer>();
}

std::unique_ptr<TextureCacheBase> VideoBackend::CreateTextureCache(AbstractGfx* gfx)
{
  return std::make_unique<TextureCache>();
}

std::string VideoBackend::GetDisplayName() const
{
  // i18n: Null is referring to the null video backend, which renders nothing
  return _trans("Null");
}
}  // namespace Null
