// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VideoBackend.h"

#include <vector>

#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PresentWait.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/VKBoundingBox.h"
#include "VideoBackends/Vulkan/VKGfx.h"
#include "VideoBackends/Vulkan/VKPerfQuery.h"
#include "VideoBackends/Vulkan/VKSwapChain.h"
#include "VideoBackends/Vulkan/VKVertexManager.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#if defined(VK_USE_PLATFORM_METAL_EXT)
#include <objc/message.h>
#endif

namespace Vulkan
{
void VideoBackend::InitBackendInfo(const WindowSystemInfo& wsi)
{
  VulkanContext::PopulateBackendInfo(&g_Config);

  if (LoadVulkanLibrary())
  {
    u32 vk_api_version = 0;
    VkInstance temp_instance = VulkanContext::CreateVulkanInstance(WindowSystemType::Headless,
                                                                   false, false, &vk_api_version);
    if (temp_instance)
    {
      if (LoadVulkanInstanceFunctions(temp_instance))
      {
        VulkanContext::GPUList gpu_list = VulkanContext::EnumerateGPUs(temp_instance);
        VulkanContext::PopulateBackendInfoAdapters(&g_Config, gpu_list);

        if (!gpu_list.empty())
        {
          // Use the selected adapter, or the first to fill features.
          size_t device_index = static_cast<size_t>(g_Config.iAdapter);
          if (device_index >= gpu_list.size())
            device_index = 0;

          VkPhysicalDevice gpu = gpu_list[device_index];
          VkPhysicalDeviceProperties properties;
          vkGetPhysicalDeviceProperties(gpu, &properties);
          VkPhysicalDeviceFeatures features;
          vkGetPhysicalDeviceFeatures(gpu, &features);
          VulkanContext::PopulateBackendInfoFeatures(&g_Config, gpu, properties, features);
          VulkanContext::PopulateBackendInfoMultisampleModes(&g_Config, gpu, properties);
        }
      }

      vkDestroyInstance(temp_instance, nullptr);
    }
    else
    {
      PanicAlertFmt("Failed to create Vulkan instance.");
    }

    UnloadVulkanLibrary();
  }
  else
  {
    PanicAlertFmt("Failed to load Vulkan library.");
  }
}

// Helper method to check whether the Host GPU logging category is enabled.
static bool IsHostGPULoggingEnabled()
{
  return Common::Log::LogManager::GetInstance()->IsEnabled(Common::Log::LogType::HOST_GPU,
                                                           Common::Log::LogLevel::LERROR);
}

// Helper method to determine whether to enable the debug utils extension.
static bool ShouldEnableDebugUtils(bool enable_validation_layers)
{
  // Enable debug utils if the Host GPU log option is checked, or validation layers are enabled.
  // The only issue here is that if Host GPU is not checked when the instance is created, the debug
  // report extension will not be enabled, requiring the game to be restarted before any reports
  // will be logged. Otherwise, we'd have to enable debug utils on every instance, when most
  // users will never check the Host GPU logging category.
  return enable_validation_layers || IsHostGPULoggingEnabled();
}

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  if (!LoadVulkanLibrary())
  {
    PanicAlertFmt("Failed to load Vulkan library.");
    return false;
  }

  // Check for presence of the validation layers before trying to enable it
  bool enable_validation_layer = g_Config.bEnableValidationLayer;
  if (enable_validation_layer && !VulkanContext::CheckValidationLayerAvailablility())
  {
    WARN_LOG_FMT(VIDEO, "Validation layer requested but not available, disabling.");
    enable_validation_layer = false;
  }

  // Create Vulkan instance, needed before we can create a surface, or enumerate devices.
  // We use this instance to fill in backend info, then re-use it for the actual device.
  bool enable_surface = wsi.type != WindowSystemType::Headless;
  bool enable_debug_utils = ShouldEnableDebugUtils(enable_validation_layer);
  u32 vk_api_version = 0;
  VkInstance instance = VulkanContext::CreateVulkanInstance(
      wsi.type, enable_debug_utils, enable_validation_layer, &vk_api_version);
  if (instance == VK_NULL_HANDLE)
  {
    PanicAlertFmt("Failed to create Vulkan instance.");
    UnloadVulkanLibrary();
    return false;
  }

  // Load instance function pointers.
  if (!LoadVulkanInstanceFunctions(instance))
  {
    PanicAlertFmt("Failed to load Vulkan instance functions.");
    vkDestroyInstance(instance, nullptr);
    UnloadVulkanLibrary();
    return false;
  }

  // Obtain a list of physical devices (GPUs) from the instance.
  // We'll re-use this list later when creating the device.
  VulkanContext::GPUList gpu_list = VulkanContext::EnumerateGPUs(instance);
  if (gpu_list.empty())
  {
    PanicAlertFmt("No Vulkan physical devices available.");
    vkDestroyInstance(instance, nullptr);
    UnloadVulkanLibrary();
    return false;
  }

  // Populate BackendInfo with as much information as we can at this point.
  VulkanContext::PopulateBackendInfo(&g_Config);
  VulkanContext::PopulateBackendInfoAdapters(&g_Config, gpu_list);

  // We need the surface before we can create a device, as some parameters depend on it.
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (enable_surface)
  {
    surface = SwapChain::CreateVulkanSurface(instance, wsi);
    if (surface == VK_NULL_HANDLE)
    {
      PanicAlertFmt("Failed to create Vulkan surface.");
      vkDestroyInstance(instance, nullptr);
      UnloadVulkanLibrary();
      return false;
    }
  }

  // Since we haven't called InitializeShared yet, iAdapter may be out of range,
  // so we have to check it ourselves.
  size_t selected_adapter_index = static_cast<size_t>(g_Config.iAdapter);
  if (selected_adapter_index >= gpu_list.size())
  {
    WARN_LOG_FMT(VIDEO, "Vulkan adapter index out of range, selecting first adapter.");
    selected_adapter_index = 0;
  }

  // Now we can create the Vulkan device. VulkanContext takes ownership of the instance and surface.
  g_vulkan_context =
      VulkanContext::Create(instance, gpu_list[selected_adapter_index], surface, enable_debug_utils,
                            enable_validation_layer, vk_api_version);
  if (!g_vulkan_context)
  {
    PanicAlertFmt("Failed to create Vulkan device");
    UnloadVulkanLibrary();
    return false;
  }

  // Since VulkanContext maintains a copy of the device features and properties, we can use this
  // to initialize the backend information, so that we don't need to enumerate everything again.
  VulkanContext::PopulateBackendInfoFeatures(&g_Config, g_vulkan_context->GetPhysicalDevice(),
                                             g_vulkan_context->GetDeviceProperties(),
                                             g_vulkan_context->GetDeviceFeatures());
  VulkanContext::PopulateBackendInfoMultisampleModes(
      &g_Config, g_vulkan_context->GetPhysicalDevice(), g_vulkan_context->GetDeviceProperties());
  g_Config.backend_info.bSupportsExclusiveFullscreen =
      enable_surface && g_vulkan_context->SupportsExclusiveFullscreen(wsi, surface);

  UpdateActiveConfig();

  // Create command buffers. We do this separately because the other classes depend on it.
  g_command_buffer_mgr = std::make_unique<CommandBufferManager>(g_Config.bBackendMultithreading, g_ActiveConfig.bVSyncActive);
  if (!g_command_buffer_mgr->Initialize())
  {
    PanicAlertFmt("Failed to create Vulkan command buffers");
    Shutdown();
    return false;
  }

  // Remaining classes are also dependent on object cache.
  g_object_cache = std::make_unique<ObjectCache>();
  if (!g_object_cache->Initialize())
  {
    PanicAlertFmt("Failed to initialize Vulkan object cache.");
    Shutdown();
    return false;
  }

  // Create swap chain. This has to be done early so that the target size is correct for auto-scale.
  std::unique_ptr<SwapChain> swap_chain;
  if (surface != VK_NULL_HANDLE)
  {
    swap_chain = SwapChain::Create(wsi, surface, g_ActiveConfig.bVSyncActive);
    if (!swap_chain)
    {
      PanicAlertFmt("Failed to create Vulkan swap chain.");
      Shutdown();
      return false;
    }

    if (g_vulkan_context->SupportsPresentWait())
      StartPresentWaitThread();
  }

  if (!StateTracker::CreateInstance())
  {
    PanicAlertFmt("Failed to create state tracker");
    Shutdown();
    return false;
  }

  auto gfx = std::make_unique<VKGfx>(std::move(swap_chain), wsi.render_surface_scale);
  auto vertex_manager = std::make_unique<VertexManager>();
  auto perf_query = std::make_unique<PerfQuery>();
  auto bounding_box = std::make_unique<VKBoundingBox>();

  return InitializeShared(std::move(gfx), std::move(vertex_manager), std::move(perf_query),
                          std::move(bounding_box));
}

void VideoBackend::Shutdown()
{
  if (g_vulkan_context->SupportsPresentWait())
    StopPresentWaitThread();

  if (g_vulkan_context)
    vkDeviceWaitIdle(g_vulkan_context->GetDevice());

  if (g_object_cache)
    g_object_cache->Shutdown();

  ShutdownShared();

  g_object_cache.reset();
  StateTracker::DestroyInstance();
  g_command_buffer_mgr.reset();
  g_vulkan_context.reset();
  UnloadVulkanLibrary();
}

void VideoBackend::PrepareWindow(WindowSystemInfo& wsi)
{
#if defined(VK_USE_PLATFORM_METAL_EXT)
  // This is kinda messy, but it avoids having to write Objective C++ just to create a metal layer.
  id view = reinterpret_cast<id>(wsi.render_surface);
  Class clsCAMetalLayer = objc_getClass("CAMetalLayer");
  if (!clsCAMetalLayer)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to get CAMetalLayer class.");
    return;
  }

  // [CAMetalLayer layer]
  id layer = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(objc_getClass("CAMetalLayer"),
                                                                sel_getUid("layer"));
  if (!layer)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to create Metal layer.");
    return;
  }

  // [view setWantsLayer:YES]
  reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(view, sel_getUid("setWantsLayer:"), YES);

  // [view setLayer:layer]
  reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(view, sel_getUid("setLayer:"), layer);

  // NSScreen* screen = [NSScreen mainScreen]
  id screen = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(objc_getClass("NSScreen"),
                                                                 sel_getUid("mainScreen"));

  // CGFloat factor = [screen backingScaleFactor]
  double factor =
      reinterpret_cast<double (*)(id, SEL)>(objc_msgSend)(screen, sel_getUid("backingScaleFactor"));

  // layer.contentsScale = factor
  reinterpret_cast<void (*)(id, SEL, double)>(objc_msgSend)(layer, sel_getUid("setContentsScale:"),
                                                            factor);

  // Store the layer pointer, that way MoltenVK doesn't call [NSView layer] outside the main thread.
  wsi.render_surface = layer;
#endif
}
}  // namespace Vulkan
