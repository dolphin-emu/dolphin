// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/VideoBackend.h"
#include "VideoBackends/Metal/CommandBufferManager.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/PerfQuery.h"
#include "VideoBackends/Metal/Render.h"
#include "VideoBackends/Metal/StateTracker.h"
#include "VideoBackends/Metal/TextureCache.h"
#include "VideoBackends/Metal/VertexManager.h"

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace Metal
{
void VideoBackend::InitBackendInfo()
{
  auto gpu_list = mtlpp::Device::CopyAllDevices();
  MetalContext::PopulateBackendInfo(&g_Config);
  MetalContext::PopulateBackendInfoAdapters(&g_Config, gpu_list);
}

bool VideoBackend::Initialize(void* window_handle)
{
  // Populate BackendInfo with as much information as we can at this point.
  MetalContext::PopulateBackendInfo(&g_Config);

  // Obtain a list of physical devices (GPUs) from the instance.
  // We'll re-use this list later when creating the device.
  auto gpu_list = mtlpp::Device::CopyAllDevices();
  mtlpp::Device device;
  if (gpu_list.GetSize() > 0)
  {
    MetalContext::PopulateBackendInfoAdapters(&g_Config, gpu_list);

    // Since we haven't called InitializeShared yet, iAdapter may be out of range,
    // so we have to check it ourselves.
    uint32_t selected_adapter_index = static_cast<uint32_t>(g_Config.iAdapter);
    if (selected_adapter_index >= gpu_list.GetSize())
    {
      WARN_LOG(VIDEO, "Metal adapter index out of range, selecting first adapter.");
      selected_adapter_index = 0;
    }
    device = gpu_list[selected_adapter_index];
  }
  else
  {
    // Try creating a default device.
    device = mtlpp::Device::CreateSystemDefaultDevice();
    if (!device)
    {
      PanicAlert("Failed to create a Metal device, does your system support Metal?");
      return false;
    }
  }

  // With the backend information populated, we can now initialize videocommon.
  INFO_LOG(VIDEO, "Metal device: %s", device.GetName().GetCStr());
  InitializeShared();

  // Create device and command buffer wrapper.
  g_metal_context = std::make_unique<MetalContext>(device);
  g_command_buffer_mgr = std::make_unique<CommandBufferManager>();
  if (!g_command_buffer_mgr->Initialize() || !g_metal_context->CreateStreamingBuffers())
  {
    Shutdown();
    return false;
  }

  std::unique_ptr<MetalFramebuffer> backbuffer;
  if (window_handle)
  {
    backbuffer = MetalFramebuffer::CreateForWindow(window_handle, AbstractTextureFormat::BGRA8);
    if (!backbuffer)
    {
      PanicAlert("Failed to create backbuffer.");
      Shutdown();
      return false;
    }
  }

  g_state_tracker = std::make_unique<StateTracker>();
  g_shader_cache = std::make_unique<VideoCommon::ShaderCache>();
  g_renderer = std::make_unique<Renderer>(std::move(backbuffer));
  g_vertex_manager = std::make_unique<VertexManager>();
  g_perf_query = std::make_unique<PerfQuery>();
  g_framebuffer_manager = std::make_unique<FramebufferManagerBase>();
  g_texture_cache = std::make_unique<TextureCache>();

  if (!g_shader_cache->Initialize() || !g_renderer->Initialize() ||
      !TextureCache::GetInstance()->Initialize() ||
      !VertexManager::GetInstance()->CreateStreamingBuffers())
  {
    PanicAlert("Failed to initialize Metal classes");
    Shutdown();
    return false;
  }

  Renderer::GetInstance()->SetInitialState();
  return true;
}

void VideoBackend::Shutdown()
{
  ShutdownShared();
  g_texture_cache.reset();
  g_perf_query.reset();
  g_vertex_manager.reset();
  g_framebuffer_manager.reset();
  if (g_renderer)
    g_renderer->Shutdown();
  g_renderer.reset();
  if (g_shader_cache)
    g_shader_cache->Shutdown();
  g_shader_cache.reset();
  g_state_tracker.reset();
  g_metal_context.reset();
}
}  // namespace Metal
