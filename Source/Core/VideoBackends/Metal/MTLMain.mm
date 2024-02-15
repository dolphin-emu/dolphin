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

#include "VideoBackends/Metal/MTLBoundingBox.h"
#include "VideoBackends/Metal/MTLGfx.h"
#include "VideoBackends/Metal/MTLObjectCache.h"
#include "VideoBackends/Metal/MTLPerfQuery.h"
#include "VideoBackends/Metal/MTLStateTracker.h"
#include "VideoBackends/Metal/MTLUtil.h"
#include "VideoBackends/Metal/MTLVertexManager.h"

#include "VideoCommon/AbstractGfx.h"
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

#if TARGET_OS_OSX
// This should be available on all macOS 13.3+ systems – but when using OCLP drivers, some devices
// fail with "Unrecognized selector -[MTLIGAccelDevice setShouldMaximizeConcurrentCompilation:]"
//
// This concerns Intel Ivy Bridge, Haswell and Nvidia Kepler on macOS 13.3 or newer.
// (See
// https://github.com/dortania/OpenCore-Legacy-Patcher/blob/34676702f494a2a789c514cc76dba19b8b7206b1/docs/PATCHEXPLAIN.md?plain=1#L354C1-L354C83)
//
// Perform the feature detection dynamically instead.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

    if ([adapter respondsToSelector:@selector(setShouldMaximizeConcurrentCompilation:)])
    {
      [adapter setShouldMaximizeConcurrentCompilation:YES];
    }

#pragma clang diagnostic pop
#endif

    UpdateActiveConfig();

    MRCOwned<CAMetalLayer*> layer = MRCRetain(static_cast<CAMetalLayer*>(wsi.render_surface));
    [layer setDevice:adapter];
    if (Util::ToAbstract([layer pixelFormat]) == AbstractTextureFormat::Undefined)
      [layer setPixelFormat:MTLPixelFormatBGRA8Unorm];

    ObjectCache::Initialize(std::move(adapter));
    g_state_tracker = std::make_unique<StateTracker>();

    return InitializeShared(
        std::make_unique<Metal::Gfx>(std::move(layer)), std::make_unique<Metal::VertexManager>(),
        std::make_unique<Metal::PerfQuery>(), std::make_unique<Metal::BoundingBox>());
  }
}

void Metal::VideoBackend::Shutdown()
{
  ShutdownShared();

  g_state_tracker.reset();
  ObjectCache::Shutdown();
}

void Metal::VideoBackend::InitBackendInfo(const WindowSystemInfo& wsi)
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
