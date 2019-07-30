// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/GL/GLContext.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/DebugUtil.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWTexture.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/TextureCache.h"
#include "VideoBackends/Software/VideoBackend.h"

#include "VideoCommon/FramebufferManager.h"
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
  return "Software Renderer";
}

std::string VideoSoftware::GetDisplayName() const
{
  return _trans("Software Renderer");
}

void VideoSoftware::InitBackendInfo()
{
  g_Config.backend_info.api_type = APIType::Nothing;
  g_Config.backend_info.MaxTextureSize = 16384;
  g_Config.backend_info.bUsesLowerLeftOrigin = false;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsEarlyZ = true;
  g_Config.backend_info.bSupportsOversizedViewports = true;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupportsGPUTextureDecoding = false;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;
  g_Config.backend_info.bSupportsCopyToVram = false;
  g_Config.backend_info.bSupportsLargePoints = false;
  g_Config.backend_info.bSupportsPartialDepthCopies = false;
  g_Config.backend_info.bSupportsFramebufferFetch = false;
  g_Config.backend_info.bSupportsBackgroundCompiling = false;
  g_Config.backend_info.bSupportsLogicOp = true;
  g_Config.backend_info.bSupportsShaderBinaries = false;
  g_Config.backend_info.bSupportsPipelineCacheData = false;

  // aamodes
  g_Config.backend_info.AAModes = {1};
}

bool VideoSoftware::Initialize(const WindowSystemInfo& wsi)
{
  InitializeShared();

  std::unique_ptr<SWOGLWindow> window = SWOGLWindow::Create(wsi);
  if (!window)
    return false;

  Clipper::Init();
  Rasterizer::Init();
  DebugUtil::Init();

  g_renderer = std::make_unique<SWRenderer>(std::move(window));
  g_vertex_manager = std::make_unique<SWVertexLoader>();
  g_shader_cache = std::make_unique<VideoCommon::ShaderCache>();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_perf_query = std::make_unique<PerfQuery>();
  g_texture_cache = std::make_unique<TextureCache>();

  if (!g_vertex_manager->Initialize() || !g_shader_cache->Initialize() ||
      !g_renderer->Initialize() || !g_framebuffer_manager->Initialize() ||
      !g_texture_cache->Initialize())
  {
    PanicAlert("Failed to initialize renderer classes");
    Shutdown();
    return false;
  }

  g_shader_cache->InitializeShaderCache();
  return true;
}

void VideoSoftware::Shutdown()
{
  if (g_shader_cache)
    g_shader_cache->Shutdown();

  if (g_renderer)
    g_renderer->Shutdown();

  DebugUtil::Shutdown();
  g_texture_cache.reset();
  g_perf_query.reset();
  g_framebuffer_manager.reset();
  g_shader_cache.reset();
  g_vertex_manager.reset();
  g_renderer.reset();
  ShutdownShared();
}
}  // namespace SW
