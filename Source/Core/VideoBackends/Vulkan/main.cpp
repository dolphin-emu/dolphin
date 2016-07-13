// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Core/Host.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/Helpers.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PerfQuery.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/VertexManager.h"
#include "VideoBackends/Vulkan/VideoBackend.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
// Can we do anything about these globals?
static VkInstance s_vkInstance;
static VkPhysicalDevice s_vkPhysicalDevice;
static VkDevice s_vkDevice;
static std::unique_ptr<SwapChain> s_swap_chain;
static std::unique_ptr<StateTracker> s_state_tracker;

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugLayerReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                         uint64_t object, size_t location, int32_t messageCode,
                         const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
  std::string log_message =
      StringFromFormat("Vulkan debug report: (%s) %s", pLayerPrefix ? pLayerPrefix : "", pMessage);
  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    GENERIC_LOG(LogTypes::HOST_GPU, LogTypes::LERROR, "%s", log_message.c_str())
  else if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
    GENERIC_LOG(LogTypes::HOST_GPU, LogTypes::LWARNING, "%s", log_message.c_str())
  else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    GENERIC_LOG(LogTypes::HOST_GPU, LogTypes::LINFO, "%s", log_message.c_str())
  else
    GENERIC_LOG(LogTypes::HOST_GPU, LogTypes::LDEBUG, "%s", log_message.c_str())

  return VK_FALSE;
}

static VkDebugReportCallbackEXT s_debug_report_callback;

static bool EnableDebugLayerReportCallback(VkInstance instance)
{
  // Check for presence of the functions before calling
  if (!vkCreateDebugReportCallbackEXT || !vkDestroyDebugReportCallbackEXT ||
      !vkDebugReportMessageEXT)
  {
    return false;
  }

  VkDebugReportCallbackCreateInfoEXT callback_info = {
      VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT, nullptr,
      VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
          VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
          VK_DEBUG_REPORT_DEBUG_BIT_EXT,
      DebugLayerReportCallback, nullptr};

  VkResult res =
      vkCreateDebugReportCallbackEXT(instance, &callback_info, nullptr, &s_debug_report_callback);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateDebugReportCallbackEXT failed: ");
    return false;
  }

  return true;
}

static void DisableDebugLayerReportCallback(VkInstance instance)
{
  if (s_debug_report_callback != VK_NULL_HANDLE)
  {
    vkDestroyDebugReportCallbackEXT(instance, s_debug_report_callback, nullptr);
    s_debug_report_callback = VK_NULL_HANDLE;
  }
}

void VideoBackend::InitBackendInfo()
{
  PopulateBackendInfo(&g_Config);

  if (LoadVulkanLibrary())
  {
    VkInstance temp_instance = CreateVulkanInstance(false);
    if (temp_instance)
    {
      if (LoadVulkanInstanceFunctions(temp_instance))
      {
        std::vector<VkPhysicalDevice> physical_devices =
            EnumerateVulkanPhysicalDevices(temp_instance);
        PopulateBackendInfoAdapters(&g_Config, physical_devices);

        if (!physical_devices.empty())
        {
          // Use the selected adapter, or the first to fill features.
          size_t device_index = static_cast<size_t>(g_Config.iAdapter);
          if (device_index >= physical_devices.size())
            device_index = 0;

          VkPhysicalDevice physical_device = physical_devices[device_index];
          PopulateBackendInfoFeatures(&g_Config, physical_device);
          PopulateBackendInfoMultisampleModes(&g_Config, physical_device);
        }
      }
    }
    else
    {
      PanicAlert("Failed to create Vulkan instance.");
    }

    vkDestroyInstance(temp_instance, nullptr);
    UnloadVulkanLibrary();
  }
  else
  {
    PanicAlert("Failed to load Vulkan library.");
  }
}

bool VideoBackend::Initialize(void* window_handle)
{
  VkSurfaceKHR surface;
  if (!LoadVulkanLibrary())
  {
    PanicAlert("Failed to load vulkan library.");
    return false;
  }

  // Base fields
  PopulateBackendInfo(&g_Config);
  InitializeShared();

  // Check for presence of the debug layer before trying to enable it
  bool enable_debug_layer = g_Config.bEnableValidationLayer;
  if (enable_debug_layer && !CheckDebugLayerAvailability())
  {
    WARN_LOG(VIDEO, "Validation layer requested but not available, disabling.");
    enable_debug_layer = false;
  }

  // Create vulkan instance
  s_vkInstance = CreateVulkanInstance(enable_debug_layer);
  if (!s_vkInstance)
  {
    PanicAlert("Failed to create vulkan instance.");
    UnloadVulkanLibrary();
    return false;
  }

  // Load instance function pointers
  if (!LoadVulkanInstanceFunctions(s_vkInstance))
  {
    PanicAlert("Failed to load vulkan instance functions.");
    UnloadVulkanLibrary();
    return false;
  }

  // Enable debug reports if debug layer is on
  if (enable_debug_layer)
    EnableDebugLayerReportCallback(s_vkInstance);

  // Create vulkan surface
  surface = CreateVulkanSurface(s_vkInstance, window_handle);
  if (!surface)
  {
    // TODO Move
    PanicAlert("Failed to create vulkan surface.");
    DisableDebugLayerReportCallback(s_vkInstance);
    UnloadVulkanLibrary();
    return false;
  }

  // Select physical device - only do this once
  std::vector<VkPhysicalDevice> physical_device_list = EnumerateVulkanPhysicalDevices(s_vkInstance);
  size_t selected_adapter_index = static_cast<size_t>(g_Config.iAdapter);
  if (physical_device_list.empty())
  {
    PanicAlert("No Vulkan physical devices available.");
    DisableDebugLayerReportCallback(s_vkInstance);
    UnloadVulkanLibrary();
    return false;
  }

  // Fill the adapter list, and check if the user has selected an invalid device
  // For some reason nvidia's driver crashes randomly if you call vkEnumeratePhysicalDevices
  // after creating a device..
  PopulateBackendInfoAdapters(&g_Config, physical_device_list);
  if (selected_adapter_index >= physical_device_list.size())
  {
    WARN_LOG(VIDEO, "Vulkan adapter index out of range, selecting first adapter.");
    selected_adapter_index = 0;
  }

  // With our physical device known we can populate the remaining backend info fields
  s_vkPhysicalDevice = physical_device_list[selected_adapter_index];
  PopulateBackendInfoFeatures(&g_Config, s_vkPhysicalDevice);
  PopulateBackendInfoMultisampleModes(&g_Config, s_vkPhysicalDevice);

  // Display the name so the user knows which device was actually created
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(s_vkPhysicalDevice, &properties);
  WARN_LOG(VIDEO, "Vulkan: Using physical adapter %s", properties.deviceName);
  OSD::AddMessage(StringFromFormat("Using physical adapter %s", properties.deviceName).c_str(),
                  5000);

  // Create vulkan device and grab queues
  uint32_t graphics_queue_family_index, present_queue_family_index;
  VkQueue graphics_queue, present_queue;
  s_vkDevice =
      CreateVulkanDevice(s_vkPhysicalDevice, surface, &graphics_queue_family_index, &graphics_queue,
                         &present_queue_family_index, &present_queue, enable_debug_layer);
  if (!s_vkDevice)
  {
    PanicAlert("Failed to create vulkan device");
    goto CLEANUP_SURFACE;
  }

  // Load device function pointers
  if (!LoadVulkanDeviceFunctions(s_vkDevice))
  {
    PanicAlert("Failed to load vulkan device functions.");
    goto CLEANUP_DEVICE;
  }

  // create command buffers
  g_command_buffer_mgr = std::make_unique<CommandBufferManager>(
      s_vkDevice, graphics_queue_family_index, graphics_queue, g_Config.bBackendMultithreading);
  if (!g_command_buffer_mgr->Initialize())
  {
    PanicAlert("Failed to create vulkan command buffers");
    goto CLEANUP_DEVICE;
  }

  // create object cache
  g_object_cache = std::make_unique<ObjectCache>(s_vkInstance, s_vkPhysicalDevice, s_vkDevice);
  if (!g_object_cache->Initialize())
  {
    PanicAlert("Failed to create vulkan object cache");
    goto CLEANUP_DEVICE;
  }

  // create swap chain and buffers
  s_swap_chain = std::make_unique<SwapChain>(window_handle, surface, present_queue);
  if (!s_swap_chain->Initialize())
  {
    PanicAlert("Failed to create vulkan swap chain");
    surface = VK_NULL_HANDLE;
    goto CLEANUP_DEVICE;
  }

  return true;

CLEANUP_DEVICE:
  s_swap_chain.reset();
  g_object_cache.reset();
  g_command_buffer_mgr.reset();

  vkDestroyDevice(s_vkDevice, nullptr);
  s_vkDevice = VK_NULL_HANDLE;

CLEANUP_SURFACE:
  if (surface != VK_NULL_HANDLE)
    vkDestroySurfaceKHR(s_vkInstance, surface, nullptr);

  DisableDebugLayerReportCallback(s_vkInstance);
  vkDestroyInstance(s_vkInstance, nullptr);
  s_vkPhysicalDevice = VK_NULL_HANDLE;
  s_vkInstance = VK_NULL_HANDLE;
  UnloadVulkanLibrary();
  return false;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
  s_state_tracker = std::make_unique<StateTracker>();
  g_vertex_manager = std::make_unique<VertexManager>(s_state_tracker.get());
  g_perf_query = std::make_unique<PerfQuery>();
  g_texture_cache = std::make_unique<TextureCache>(s_state_tracker.get());
  g_renderer = std::make_unique<Renderer>(s_swap_chain.get(), s_state_tracker.get());
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  if (!static_cast<Renderer*>(g_renderer.get())->Initialize())
    PanicAlert("Failed to initialize renderer");
}

void VideoBackend::Shutdown()
{
  g_command_buffer_mgr->WaitForGPUIdle();

  s_swap_chain.reset();
  g_object_cache->Shutdown();
  g_object_cache.reset();
  g_command_buffer_mgr.reset();

  vkDestroyDevice(s_vkDevice, nullptr);
  s_vkDevice = nullptr;

  DisableDebugLayerReportCallback(s_vkInstance);
  vkDestroyInstance(s_vkInstance, nullptr);
  s_vkInstance = nullptr;

  UnloadVulkanLibrary();

  ShutdownShared();
}

void VideoBackend::Video_Cleanup()
{
  vkDeviceWaitIdle(s_vkDevice);

  g_texture_cache.reset();
  g_perf_query.reset();
  g_vertex_manager.reset();
  g_renderer.reset();
  g_framebuffer_manager.reset();

  s_state_tracker.reset();

  CleanupShared();
}
}
