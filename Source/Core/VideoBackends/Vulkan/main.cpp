// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"

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
          VulkanContext::PopulateBackendInfoFeatures(&g_Config, gpu, properties, features);
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

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  if (!LoadVulkanLibrary())
  {
    PanicAlert("Failed to load Vulkan library.");
    return false;
  }

  // Check for presence of the validation layers before trying to enable it
  bool enable_validation_layer = g_Config.bEnableValidationLayer;
  if (enable_validation_layer && !VulkanContext::CheckValidationLayerAvailablility())
  {
    WARN_LOG(VIDEO, "Validation layer requested but not available, disabling.");
    enable_validation_layer = false;
  }

  // Create Vulkan instance, needed before we can create a surface, or enumerate devices.
  // We use this instance to fill in backend info, then re-use it for the actual device.
  bool enable_surface = wsi.render_surface != nullptr;
  bool enable_debug_reports = ShouldEnableDebugReports(enable_validation_layer);
  VkInstance instance = VulkanContext::CreateVulkanInstance(enable_surface, enable_debug_reports,
                                                            enable_validation_layer);
  if (instance == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to create Vulkan instance.");
    UnloadVulkanLibrary();
    return false;
  }

  // Load instance function pointers.
  if (!LoadVulkanInstanceFunctions(instance))
  {
    PanicAlert("Failed to load Vulkan instance functions.");
    vkDestroyInstance(instance, nullptr);
    UnloadVulkanLibrary();
    return false;
  }

  // Obtain a list of physical devices (GPUs) from the instance.
  // We'll re-use this list later when creating the device.
  VulkanContext::GPUList gpu_list = VulkanContext::EnumerateGPUs(instance);
  if (gpu_list.empty())
  {
    PanicAlert("No Vulkan physical devices available.");
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
    surface = SwapChain::CreateVulkanSurface(instance, wsi.display_connection, wsi.render_surface);
    if (surface == VK_NULL_HANDLE)
    {
      PanicAlert("Failed to create Vulkan surface.");
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
    WARN_LOG(VIDEO, "Vulkan adapter index out of range, selecting first adapter.");
    selected_adapter_index = 0;
  }

  // Now we can create the Vulkan device. VulkanContext takes ownership of the instance and surface.
  g_vulkan_context = VulkanContext::Create(instance, gpu_list[selected_adapter_index], surface,
                                           enable_debug_reports, enable_validation_layer);
  if (!g_vulkan_context)
  {
    PanicAlert("Failed to create Vulkan device");
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

  // With the backend information populated, we can now initialize videocommon.
  InitializeShared();

  // Create command buffers. We do this separately because the other classes depend on it.
  g_command_buffer_mgr = std::make_unique<CommandBufferManager>(g_Config.bBackendMultithreading);
  if (!g_command_buffer_mgr->Initialize())
  {
    PanicAlert("Failed to create Vulkan command buffers");
    Shutdown();
    return false;
  }

  // Remaining classes are also dependent on object/shader cache.
  g_object_cache = std::make_unique<ObjectCache>();
  g_shader_cache = std::make_unique<ShaderCache>();
  if (!g_object_cache->Initialize() || !g_shader_cache->Initialize())
  {
    PanicAlert("Failed to initialize Vulkan object cache.");
    Shutdown();
    return false;
  }

  // Create swap chain. This has to be done early so that the target size is correct for auto-scale.
  std::unique_ptr<SwapChain> swap_chain;
  if (surface != VK_NULL_HANDLE)
  {
    swap_chain =
        SwapChain::Create(wsi.display_connection, wsi.render_surface, surface, g_Config.IsVSync());
    if (!swap_chain)
    {
      PanicAlert("Failed to create Vulkan swap chain.");
      Shutdown();
      return false;
    }
  }

  // Create main wrapper instances.
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_renderer = std::make_unique<Renderer>(std::move(swap_chain));
  g_vertex_manager = std::make_unique<VertexManager>();
  g_texture_cache = std::make_unique<TextureCache>();
  ::g_shader_cache = std::make_unique<VideoCommon::ShaderCache>();
  g_perf_query = std::make_unique<PerfQuery>();

  // Invoke init methods on main wrapper classes.
  // These have to be done before the others because the destructors
  // for the remaining classes may call methods on these.
  if (!StateTracker::CreateInstance() || !FramebufferManager::GetInstance()->Initialize() ||
      !Renderer::GetInstance()->Initialize() || !VertexManager::GetInstance()->Initialize() ||
      !TextureCache::GetInstance()->Initialize() || !PerfQuery::GetInstance()->Initialize() ||
      !::g_shader_cache->Initialize())
  {
    PanicAlert("Failed to initialize Vulkan classes.");
    Shutdown();
    return false;
  }

  // Display the name so the user knows which device was actually created.
  INFO_LOG(VIDEO, "Vulkan Device: %s", g_vulkan_context->GetDeviceProperties().deviceName);
  return true;
}

void VideoBackend::Shutdown()
{
  if (g_command_buffer_mgr)
    g_command_buffer_mgr->WaitForGPUIdle();

  if (::g_shader_cache)
    ::g_shader_cache->Shutdown();

  if (g_renderer)
    g_renderer->Shutdown();

  g_perf_query.reset();
  ::g_shader_cache.reset();
  g_texture_cache.reset();
  g_vertex_manager.reset();
  g_renderer.reset();
  g_framebuffer_manager.reset();
  StateTracker::DestroyInstance();
  if (g_shader_cache)
    g_shader_cache->Shutdown();
  g_shader_cache.reset();
  g_object_cache.reset();
  g_command_buffer_mgr.reset();
  g_vulkan_context.reset();
  ShutdownShared();
  UnloadVulkanLibrary();
}
}  // namespace Vulkan
