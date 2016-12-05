// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/Logging/LogManager.h"
#include "Core/Host.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PerfQuery.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/VertexManager.h"
#include "VideoBackends/Vulkan/VideoBackend.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
void VideoBackend::InitBackendInfo()
{
  VulkanContext::PopulateBackendInfo(&g_Config);

  if (LoadVulkanLibrary())
  {
    VkInstance temp_instance = VulkanContext::CreateVulkanInstance(false, false, false);
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
          VulkanContext::PopulateBackendInfoFeatures(&g_Config, gpu, features);
          VulkanContext::PopulateBackendInfoMultisampleModes(&g_Config, gpu, properties);
        }
      }

      vkDestroyInstance(temp_instance, nullptr);
    }
    else
    {
      PanicAlert("Failed to create Vulkan instance.");
    }

    UnloadVulkanLibrary();
  }
  else
  {
    PanicAlert("Failed to load Vulkan library.");
  }
}

// Helper method to check whether the Host GPU logging category is enabled.
static bool IsHostGPULoggingEnabled()
{
  return LogManager::GetInstance()->IsEnabled(LogTypes::HOST_GPU, LogTypes::LERROR);
}

// Helper method to determine whether to enable the debug report extension.
static bool ShouldEnableDebugReports(bool enable_validation_layers)
{
  // Enable debug reports if the Host GPU log option is checked, or validation layers are enabled.
  // The only issue here is that if Host GPU is not checked when the instance is created, the debug
  // report extension will not be enabled, requiring the game to be restarted before any reports
  // will be logged. Otherwise, we'd have to enable debug reports on every instance, when most
  // users will never check the Host GPU logging category.
  return enable_validation_layers || IsHostGPULoggingEnabled();
}

bool VideoBackend::Initialize(void* window_handle)
{
  if (!LoadVulkanLibrary())
  {
    PanicAlert("Failed to load Vulkan library.");
    return false;
  }

  // HACK: Use InitBackendInfo to initially populate backend features.
  // This is because things like stereo get disabled when the config is validated,
  // which happens before our device is created (settings control instance behavior),
  // and we don't want that to happen if the device actually supports it.
  InitBackendInfo();
  InitializeShared();

  // Check for presence of the validation layers before trying to enable it
  bool enable_validation_layer = g_Config.bEnableValidationLayer;
  if (enable_validation_layer && !VulkanContext::CheckValidationLayerAvailablility())
  {
    WARN_LOG(VIDEO, "Validation layer requested but not available, disabling.");
    enable_validation_layer = false;
  }

  // Create Vulkan instance, needed before we can create a surface.
  bool enable_surface = window_handle != nullptr;
  bool enable_debug_reports = ShouldEnableDebugReports(enable_validation_layer);
  VkInstance instance = VulkanContext::CreateVulkanInstance(enable_surface, enable_debug_reports,
                                                            enable_validation_layer);
  if (instance == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to create Vulkan instance.");
    UnloadVulkanLibrary();
    ShutdownShared();
    return false;
  }

  // Load instance function pointers
  if (!LoadVulkanInstanceFunctions(instance))
  {
    PanicAlert("Failed to load Vulkan instance functions.");
    vkDestroyInstance(instance, nullptr);
    UnloadVulkanLibrary();
    ShutdownShared();
    return false;
  }

  // Create Vulkan surface
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (enable_surface)
  {
    surface = SwapChain::CreateVulkanSurface(instance, window_handle);
    if (surface == VK_NULL_HANDLE)
    {
      PanicAlert("Failed to create Vulkan surface.");
      vkDestroyInstance(instance, nullptr);
      UnloadVulkanLibrary();
      ShutdownShared();
      return false;
    }
  }

  // Fill the adapter list, and check if the user has selected an invalid device
  // For some reason nvidia's driver crashes randomly if you call vkEnumeratePhysicalDevices
  // after creating a device..
  VulkanContext::GPUList gpu_list = VulkanContext::EnumerateGPUs(instance);
  size_t selected_adapter_index = static_cast<size_t>(g_Config.iAdapter);
  if (gpu_list.empty())
  {
    PanicAlert("No Vulkan physical devices available.");
    if (surface != VK_NULL_HANDLE)
      vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyInstance(instance, nullptr);
    UnloadVulkanLibrary();
    ShutdownShared();
    return false;
  }
  else if (selected_adapter_index >= gpu_list.size())
  {
    WARN_LOG(VIDEO, "Vulkan adapter index out of range, selecting first adapter.");
    selected_adapter_index = 0;
  }

  // Pass ownership over to VulkanContext, and let it take care of everything.
  g_vulkan_context =
      VulkanContext::Create(instance, gpu_list[selected_adapter_index], surface, &g_Config,
                            enable_debug_reports, enable_validation_layer);
  if (!g_vulkan_context)
  {
    PanicAlert("Failed to create Vulkan device");
    UnloadVulkanLibrary();
    ShutdownShared();
    return false;
  }

  // Create swap chain. This has to be done early so that the target size is correct for auto-scale.
  std::unique_ptr<SwapChain> swap_chain;
  if (surface != VK_NULL_HANDLE)
  {
    swap_chain = SwapChain::Create(window_handle, surface, g_Config.IsVSync());
    if (!swap_chain)
    {
      PanicAlert("Failed to create Vulkan swap chain.");
      return false;
    }
  }

  // Create command buffers. We do this separately because the other classes depend on it.
  g_command_buffer_mgr = std::make_unique<CommandBufferManager>(g_Config.bBackendMultithreading);
  if (!g_command_buffer_mgr->Initialize())
  {
    PanicAlert("Failed to create Vulkan command buffers");
    g_command_buffer_mgr.reset();
    g_vulkan_context.reset();
    UnloadVulkanLibrary();
    ShutdownShared();
    return false;
  }

  // Create main wrapper instances.
  g_object_cache = std::make_unique<ObjectCache>();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_renderer = std::make_unique<Renderer>(std::move(swap_chain));

  // Invoke init methods on main wrapper classes.
  // These have to be done before the others because the destructors
  // for the remaining classes may call methods on these.
  if (!g_object_cache->Initialize() || !FramebufferManager::GetInstance()->Initialize() ||
      !StateTracker::CreateInstance() || !Renderer::GetInstance()->Initialize())
  {
    PanicAlert("Failed to initialize Vulkan classes.");
    g_renderer.reset();
    StateTracker::DestroyInstance();
    g_framebuffer_manager.reset();
    g_object_cache.reset();
    g_command_buffer_mgr.reset();
    g_vulkan_context.reset();
    UnloadVulkanLibrary();
    ShutdownShared();
    return false;
  }

  // Create remaining wrapper instances.
  g_vertex_manager = std::make_unique<VertexManager>();
  g_texture_cache = std::make_unique<TextureCache>();
  g_perf_query = std::make_unique<PerfQuery>();
  if (!VertexManager::GetInstance()->Initialize() || !TextureCache::GetInstance()->Initialize() ||
      !PerfQuery::GetInstance()->Initialize())
  {
    PanicAlert("Failed to initialize Vulkan classes.");
    g_perf_query.reset();
    g_texture_cache.reset();
    g_vertex_manager.reset();
    g_renderer.reset();
    StateTracker::DestroyInstance();
    g_framebuffer_manager.reset();
    g_object_cache.reset();
    g_command_buffer_mgr.reset();
    g_vulkan_context.reset();
    UnloadVulkanLibrary();
    ShutdownShared();
    return false;
  }

  return true;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
  // Display the name so the user knows which device was actually created
  OSD::AddMessage(StringFromFormat("Using physical adapter %s",
                                   g_vulkan_context->GetDeviceProperties().deviceName)
                      .c_str(),
                  5000);
}

void VideoBackend::Shutdown()
{
  g_command_buffer_mgr->WaitForGPUIdle();

  g_object_cache.reset();
  g_command_buffer_mgr.reset();
  g_vulkan_context.reset();

  UnloadVulkanLibrary();

  ShutdownShared();
}

void VideoBackend::Video_Cleanup()
{
  g_command_buffer_mgr->WaitForGPUIdle();

  // Save all cached pipelines out to disk for next time.
  g_object_cache->SavePipelineCache();

  g_perf_query.reset();
  g_texture_cache.reset();
  g_vertex_manager.reset();
  g_framebuffer_manager.reset();
  StateTracker::DestroyInstance();
  g_renderer.reset();

  CleanupShared();
}
}
