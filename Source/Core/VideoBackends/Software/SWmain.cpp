// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/VideoBackend.h"

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/GL/GLContext.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWBoundingBox.h"
#include "VideoBackends/Software/SWGfx.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWTexture.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/TextureCache.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace SW
{
class PerfQuery : public PerfQueryBase
{
public:
  PerfQuery() {}
  ~PerfQuery() {}
  void EnableQuery(PerfQueryGroup type) override {}
  void DisableQuery(PerfQueryGroup type) override {}
  void ResetQuery() override { EfbInterface::ResetPerfQuery(); }
  u32 GetQueryResult(PerfQueryType type) override { return EfbInterface::GetPerfQueryResult(type); }
  void FlushResults() override {}
  bool IsFlushed() const override { return true; }
};

std::string VideoSoftware::GetName() const
{
  return NAME;
}

std::string VideoSoftware::GetDisplayName() const
{
  return _trans("Software Renderer");
}

std::optional<std::string> VideoSoftware::GetWarningMessage() const
{
  return _trans("The software renderer is significantly slower than other "
                "backends and is only recommended for debugging purposes.\n\nDo you "
                "really want to enable software rendering? If unsure, select 'No'.");
}

void VideoSoftware::InitBackendInfo(const WindowSystemInfo& wsi)
{
  g_Config.backend_info.api_type = APIType::Nothing;
  g_Config.backend_info.MaxTextureSize = 16384;
  g_Config.backend_info.bUsesLowerLeftOrigin = false;
  g_Config.backend_info.bSupports3DVision = false;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsEarlyZ = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = false;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupportsGPUTextureDecoding = false;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;
  g_Config.backend_info.bSupportsCopyToVram = false;
  g_Config.backend_info.bSupportsLargePoints = false;
  g_Config.backend_info.bSupportsDepthReadback = false;
  g_Config.backend_info.bSupportsPartialDepthCopies = false;
  g_Config.backend_info.bSupportsFramebufferFetch = false;
  g_Config.backend_info.bSupportsBackgroundCompiling = false;
  g_Config.backend_info.bSupportsLogicOp = true;
  g_Config.backend_info.bSupportsShaderBinaries = false;
  g_Config.backend_info.bSupportsPipelineCacheData = false;
  g_Config.backend_info.bSupportsBBox = true;
  g_Config.backend_info.bSupportsCoarseDerivatives = false;
  g_Config.backend_info.bSupportsTextureQueryLevels = false;
  g_Config.backend_info.bSupportsLodBiasInSampler = false;
  g_Config.backend_info.bSupportsSettingObjectNames = false;
  g_Config.backend_info.bSupportsPartialMultisampleResolve = true;
  g_Config.backend_info.bSupportsDynamicVertexLoader = false;

  // aamodes
  g_Config.backend_info.AAModes = {1};
}

bool VideoSoftware::Initialize(const WindowSystemInfo& wsi)
{
  std::unique_ptr<SWOGLWindow> window = SWOGLWindow::Create(wsi);
  if (!window)
    return false;

  Clipper::Init();
  Rasterizer::Init();

  return InitializeShared(std::make_unique<SWGfx>(std::move(window)),
                          std::make_unique<SWVertexLoader>(), std::make_unique<PerfQuery>(),
                          std::make_unique<SWBoundingBox>(), std::make_unique<SWRenderer>(),
                          std::make_unique<TextureCache>());
}

void VideoSoftware::Shutdown()
{
  ShutdownShared();
}
}  // namespace SW
