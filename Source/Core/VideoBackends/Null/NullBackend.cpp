// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Null Backend Documentation

// This backend tries not to do anything in the backend,
// but everything in VideoCommon.

#include "VideoBackends/Null/FramebufferManager.h"
#include "VideoBackends/Null/PerfQuery.h"
#include "VideoBackends/Null/Render.h"
#include "VideoBackends/Null/ShaderCache.h"
#include "VideoBackends/Null/TextureCache.h"
#include "VideoBackends/Null/VertexManager.h"
#include "VideoBackends/Null/VideoBackend.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace Null
{
void VideoBackend::InitBackendInfo()
{
  g_Config.backend_info.api_type = APIType::Nothing;
  g_Config.backend_info.MaxTextureSize = 16384;
  g_Config.backend_info.bSupportsExclusiveFullscreen = true;
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = true;
  g_Config.backend_info.bSupportsOversizedViewports = true;
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
  g_Config.backend_info.bSupportsInternalResolutionFrameDumps = false;
  g_Config.backend_info.bSupportsGPUTextureDecoding = false;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;

  // aamodes: We only support 1 sample, so no MSAA
  g_Config.backend_info.Adapters.clear();
  g_Config.backend_info.AAModes = {1};
}

bool VideoBackend::Initialize(void* window_handle)
{
  InitializeShared();
  InitBackendInfo();

  return true;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
  g_renderer = std::make_unique<Renderer>();
  g_vertex_manager = std::make_unique<VertexManager>();
  g_perf_query = std::make_unique<PerfQuery>();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_texture_cache = std::make_unique<TextureCache>();
  VertexShaderCache::s_instance = std::make_unique<VertexShaderCache>();
  GeometryShaderCache::s_instance = std::make_unique<GeometryShaderCache>();
  PixelShaderCache::s_instance = std::make_unique<PixelShaderCache>();
}

void VideoBackend::Shutdown()
{
  ShutdownShared();
}

void VideoBackend::Video_Cleanup()
{
  CleanupShared();
  PixelShaderCache::s_instance.reset();
  VertexShaderCache::s_instance.reset();
  GeometryShaderCache::s_instance.reset();
  g_texture_cache.reset();
  g_perf_query.reset();
  g_vertex_manager.reset();
  g_framebuffer_manager.reset();
  g_renderer.reset();
}
}
