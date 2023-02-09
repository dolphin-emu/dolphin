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

void VideoSoftware::InitBackendInfo()
{
  backend_info.api_type = APIType::Nothing;
  backend_info.MaxTextureSize = 16384;
  backend_info.bUsesLowerLeftOrigin = false;
  backend_info.bSupports3DVision = false;
  backend_info.bSupportsDualSourceBlend = true;
  backend_info.bSupportsEarlyZ = true;
  backend_info.bSupportsPrimitiveRestart = false;
  backend_info.bSupportsMultithreading = false;
  backend_info.bSupportsComputeShaders = false;
  backend_info.bSupportsGPUTextureDecoding = false;
  backend_info.bSupportsST3CTextures = false;
  backend_info.bSupportsBPTCTextures = false;
  backend_info.bSupportsCopyToVram = false;
  backend_info.bSupportsLargePoints = false;
  backend_info.bSupportsDepthReadback = false;
  backend_info.bSupportsPartialDepthCopies = false;
  backend_info.bSupportsFramebufferFetch = false;
  backend_info.bSupportsBackgroundCompiling = false;
  backend_info.bSupportsLogicOp = true;
  backend_info.bSupportsShaderBinaries = false;
  backend_info.bSupportsPipelineCacheData = false;
  backend_info.bSupportsBBox = true;
  backend_info.bSupportsCoarseDerivatives = false;
  backend_info.bSupportsTextureQueryLevels = false;
  backend_info.bSupportsLodBiasInSampler = false;
  backend_info.bSupportsSettingObjectNames = false;
  backend_info.bSupportsPartialMultisampleResolve = true;
  backend_info.bSupportsDynamicVertexLoader = false;

  // aamodes
  backend_info.AAModes = {1};
}

std::unique_ptr<AbstractGfx> VideoSoftware::CreateGfx()
{
  std::unique_ptr<SWOGLWindow> window = SWOGLWindow::Create(m_wsi);
  if (!window)
    return {};

  Clipper::Init();
  Rasterizer::Init();

  return std::make_unique<SWGfx>(this, std::move(window));
}

std::unique_ptr<VertexManagerBase> VideoSoftware::CreateVertexManager(AbstractGfx* gfx)
{
  return std::make_unique<SWVertexLoader>();
}

std::unique_ptr<PerfQueryBase> VideoSoftware::CreatePerfQuery(AbstractGfx* gfx)
{
  return std::make_unique<SW::PerfQuery>();
}

std::unique_ptr<BoundingBox> VideoSoftware::CreateBoundingBox(AbstractGfx* gfx)
{
  return std::make_unique<SWBoundingBox>();
}

std::unique_ptr<Renderer> VideoSoftware::CreateRenderer(AbstractGfx* gfx)
{
  return std::make_unique<SWRenderer>();
}

std::unique_ptr<TextureCacheBase> VideoSoftware::CreateTextureCache(AbstractGfx* gfx)
{
  return std::make_unique<SW::TextureCache>();
}

}  // namespace SW
