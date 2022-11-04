// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/VideoBackend.h"

// This must be included before we use any TARGET_OS_* macros.
#include <TargetConditionals.h>

#if TARGET_OS_OSX
#include <AppKit/AppKit.h>
#endif

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include "Common/Common.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Metal/MTLObjectCache.h"
#include "VideoBackends/Metal/MTLPerfQuery.h"
#include "VideoBackends/Metal/MTLRenderer.h"
#include "VideoBackends/Metal/MTLStateTracker.h"
#include "VideoBackends/Metal/MTLUtil.h"
#include "VideoBackends/Metal/MTLVertexManager.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

std::string Metal::VideoBackend::GetName() const
{
  return NAME;
}

std::string Metal::VideoBackend::GetDisplayName() const
{
  // i18n: Apple's Metal graphics API (https://developer.apple.com/metal/)
  return _trans("Metal");
}

std::optional<std::string> Metal::VideoBackend::GetWarningMessage() const
{
  if (Util::GetAdapterList().empty())
  {
    return _trans("No Metal-compatible GPUs were found.  "
                  "Use the OpenGL backend or upgrade your computer/GPU");
  }

  return std::nullopt;
}

static bool WindowSystemTypeSupportsMetal(WindowSystemType type)
{
  switch (type)
  {
  case WindowSystemType::MacOS:
  case WindowSystemType::Headless:
    return true;
  default:
    return false;
  }
}

bool Metal::VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  @autoreleasepool
  {
    const bool surface_ok = wsi.type == WindowSystemType::Headless || wsi.render_surface;
    if (!WindowSystemTypeSupportsMetal(wsi.type) || !surface_ok)
    {
      PanicAlertFmt("Bad WindowSystemInfo for Metal renderer.");
      return false;
    }

    auto devs = Util::GetAdapterList();
    if (devs.empty())
    {
      PanicAlertFmt("No Metal GPUs detected.");
      return false;
    }

    Util::PopulateBackendInfo(&g_Config);
    Util::PopulateBackendInfoAdapters(&g_Config, devs);

    // Since we haven't called InitializeShared yet, iAdapter may be out of range,
    // so we have to check it ourselves.
    size_t selected_adapter_index = static_cast<size_t>(g_Config.iAdapter);
    if (selected_adapter_index >= devs.size())
    {
      WARN_LOG_FMT(VIDEO, "Metal adapter index out of range, selecting default adapter.");
      selected_adapter_index = 0;
    }
    MRCOwned<id<MTLDevice>> adapter = std::move(devs[selected_adapter_index]);
    Util::PopulateBackendInfoFeatures(&g_Config, adapter);

    // With the backend information populated, we can now initialize videocommon.
    InitializeShared();

    MRCOwned<CAMetalLayer*> layer = MRCRetain(static_cast<CAMetalLayer*>(wsi.render_surface));
    [layer setDevice:adapter];
    if (Util::ToAbstract([layer pixelFormat]) == AbstractTextureFormat::Undefined)
      [layer setPixelFormat:MTLPixelFormatBGRA8Unorm];
    CGSize size = [layer bounds].size;
    float scale = [layer contentsScale];
    if (!layer)  // headless
      scale = 1.0;

    ObjectCache::Initialize(std::move(adapter));
    g_state_tracker = std::make_unique<StateTracker>();
    g_renderer = std::make_unique<Renderer>(std::move(layer), size.width * scale,
                                            size.height * scale, scale);
    g_vertex_manager = std::make_unique<VertexManager>();
    g_perf_query = std::make_unique<PerfQuery>();
    g_framebuffer_manager = std::make_unique<FramebufferManager>();
    g_texture_cache = std::make_unique<TextureCacheBase>();
    g_shader_cache = std::make_unique<VideoCommon::ShaderCache>();

    if (!g_vertex_manager->Initialize() || !g_shader_cache->Initialize() ||
        !g_renderer->Initialize() || !g_framebuffer_manager->Initialize() ||
        !g_texture_cache->Initialize())
    {
      PanicAlertFmt("Failed to initialize renderer classes");
      Shutdown();
      return false;
    }

    g_shader_cache->InitializeShaderCache();

    return true;
  }
}

void Metal::VideoBackend::Shutdown()
{
  g_shader_cache->Shutdown();
  g_renderer->Shutdown();

  g_shader_cache.reset();
  g_texture_cache.reset();
  g_framebuffer_manager.reset();
  g_perf_query.reset();
  g_vertex_manager.reset();
  g_renderer.reset();
  g_state_tracker.reset();
  ObjectCache::Shutdown();
  ShutdownShared();
}

void Metal::VideoBackend::InitBackendInfo()
{
  @autoreleasepool
  {
    Util::PopulateBackendInfo(&g_Config);
    auto adapters = Util::GetAdapterList();
    Util::PopulateBackendInfoAdapters(&g_Config, adapters);
    if (!adapters.empty())
    {
      // Use the selected adapter, or the first to fill features.
      size_t index = static_cast<size_t>(g_Config.iAdapter);
      if (index >= adapters.size())
        index = 0;
      Util::PopulateBackendInfoFeatures(&g_Config, adapters[index]);
    }
  }
}

void Metal::VideoBackend::PrepareWindow(WindowSystemInfo& wsi)
{
#if TARGET_OS_OSX
  if (wsi.type != WindowSystemType::MacOS)
    return;
  NSView* view = static_cast<NSView*>(wsi.render_surface);
  CAMetalLayer* layer = [CAMetalLayer layer];
  [view setWantsLayer:YES];
  [view setLayer:layer];
  wsi.render_surface = layer;
#endif
}
