// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"

#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/DebugUtil.h"
#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWTexture.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/VideoBackend.h"

#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

#define VSYNC_ENABLED 0

namespace SW
{
class PerfQuery : public PerfQueryBase
{
public:
  PerfQuery() {}
  ~PerfQuery() {}
  void EnableQuery(PerfQueryGroup type) override {}
  void DisableQuery(PerfQueryGroup type) override {}
  void ResetQuery() override
  {
    memset(EfbInterface::perf_values, 0, sizeof(EfbInterface::perf_values));
  }
  u32 GetQueryResult(PerfQueryType type) override { return EfbInterface::perf_values[type]; }
  void FlushResults() override {}
  bool IsFlushed() const override { return true; }
};

class TextureCache : public TextureCacheBase
{
public:
  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}
  void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, const void* palette,
                      TLUTFormat format) override
  {
  }
  void CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
               u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half) override
  {
    EfbCopy::CopyEfb();
  }

private:
  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override
  {
    return std::make_unique<SWTexture>(config);
  }

  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, unsigned int cbuf_id, const float* colmat) override
  {
    EfbCopy::CopyEfb();
  }
};

class XFBSource : public XFBSourceBase
{
  void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight) override {}
  void CopyEFB(float Gamma) override {}
};

class FramebufferManager : public FramebufferManagerBase
{
  std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width,
                                                 unsigned int target_height,
                                                 unsigned int layers) override
  {
    return std::make_unique<XFBSource>();
  }

  std::pair<u32, u32> GetTargetSize() const override { return std::make_pair(0, 0); }
  void CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc,
                     float Gamma = 1.0f) override
  {
    EfbCopy::CopyEfb();
  }
};

std::string VideoSoftware::GetName() const
{
  return "Software Renderer";
}

std::string VideoSoftware::GetDisplayName() const
{
  return "Software Renderer";
}

void VideoSoftware::InitBackendInfo()
{
  g_Config.backend_info.api_type = APIType::Nothing;
  g_Config.backend_info.MaxTextureSize = 16384;
  g_Config.backend_info.bSupports3DVision = false;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsEarlyZ = true;
  g_Config.backend_info.bSupportsOversizedViewports = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = false;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupportsInternalResolutionFrameDumps = false;
  g_Config.backend_info.bSupportsGPUTextureDecoding = false;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;

  // aamodes
  g_Config.backend_info.AAModes = {1};
}

bool VideoSoftware::Initialize(void* window_handle)
{
  InitBackendInfo();
  InitializeShared();

  SWOGLWindow::Init(window_handle);

  Clipper::Init();
  Rasterizer::Init();
  SWRenderer::Init();
  DebugUtil::Init();

  return true;
}

void VideoSoftware::Shutdown()
{
  SWOGLWindow::Shutdown();

  ShutdownShared();
}

void VideoSoftware::Video_Cleanup()
{
  CleanupShared();

  SWRenderer::Shutdown();
  DebugUtil::Shutdown();
  // The following calls are NOT Thread Safe
  // And need to be called from the video thread
  SWRenderer::Shutdown();
  g_framebuffer_manager.reset();
  g_texture_cache.reset();
  g_perf_query.reset();
  g_vertex_manager.reset();
  g_renderer.reset();
}

// This is called after Video_Initialize() from the Core
void VideoSoftware::Video_Prepare()
{
  g_renderer = std::make_unique<SWRenderer>();
  g_vertex_manager = std::make_unique<SWVertexLoader>();
  g_perf_query = std::make_unique<PerfQuery>();
  g_texture_cache = std::make_unique<TextureCache>();
  SWRenderer::Init();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
}

unsigned int VideoSoftware::PeekMessages()
{
  return SWOGLWindow::s_instance->PeekMessages();
}
}
