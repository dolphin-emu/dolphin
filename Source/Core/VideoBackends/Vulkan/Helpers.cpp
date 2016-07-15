// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/Vulkan/Helpers.h"

#include "VideoCommon/VideoConfig.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#include <Windows.h>
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#endif

namespace Vulkan
{
VkInstance CreateVulkanInstance(bool enable_debug_layer)
{
  std::vector<const char*> enabled_extensions = SelectVulkanInstanceExtensions(enable_debug_layer);
  if (enabled_extensions.empty())
    return nullptr;

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = nullptr;
  app_info.pApplicationName = "Dolphin Emulator";
  app_info.applicationVersion = VK_MAKE_VERSION(5, 0, 0);
  app_info.pEngineName = "Dolphin Emulator";
  app_info.engineVersion = VK_MAKE_VERSION(5, 0, 0);
  app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  VkInstanceCreateInfo instance_create_info = {};
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pNext = nullptr;
  instance_create_info.flags = 0;
  instance_create_info.pApplicationInfo = &app_info;
  instance_create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
  instance_create_info.ppEnabledExtensionNames = enabled_extensions.data();
  instance_create_info.enabledLayerCount = 0;
  instance_create_info.ppEnabledLayerNames = nullptr;

  // Enable debug layer on debug builds
  if (enable_debug_layer)
  {
    static const char* layer_names[] = {"VK_LAYER_LUNARG_standard_validation"};
    instance_create_info.enabledLayerCount = 1;
    instance_create_info.ppEnabledLayerNames = layer_names;
  }

  VkInstance instance;
  VkResult res = vkCreateInstance(&instance_create_info, nullptr, &instance);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateInstance failed: ");
    return nullptr;
  }

  return instance;
}

// At least one extension (surface) is required, so an empty vector returned by this function is an
// error case.
std::vector<const char*> SelectVulkanInstanceExtensions(bool enable_debug_layer)
{
  uint32_t extension_count;
  VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
    return {};
  }

  if (extension_count == 0)
  {
    ERROR_LOG(VIDEO, "Vulkan: No extensions supported by instance.");
    return {};
  }

  std::vector<VkExtensionProperties> extension_list(extension_count);
  res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_list.data());
  assert(res == VK_SUCCESS);

  for (const auto& extension_properties : extension_list)
    INFO_LOG(VIDEO, "Available extension: %s", extension_properties.extensionName);

  std::vector<const char*> enabled_extensions;
  auto CheckForExtension = [&](const char* name, bool required) -> bool {
    if (std::find_if(extension_list.begin(), extension_list.end(),
                     [&](const VkExtensionProperties& properties) {
                       return !strcmp(name, properties.extensionName);
                     }) != extension_list.end())
    {
      INFO_LOG(VIDEO, "Enabling extension: %s", name);
      enabled_extensions.push_back(name);
      return true;
    }

    if (required)
    {
      ERROR_LOG(VIDEO, "Vulkan: Missing required extension %s.", name);
      return false;
    }

    return true;
  };

  // Common extensions
  if (!CheckForExtension(VK_KHR_SURFACE_EXTENSION_NAME, true))
  {
    return {};
  }

#if defined(VK_USE_PLATFORM_WIN32_KHR)
  if (!CheckForExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true))
    return {};
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
  if (!CheckForExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME, true))
    return {};
#elif defined(VK_USE_PLATFORM_XCB_KHR)
  if (!CheckForExtension(VK_KHR_XCB_SURFACE_EXTENSION_NAME, true))
    return {};
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
  if (!CheckForExtension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, true))
    return {};
#endif

  // VK_EXT_debug_report
  if (enable_debug_layer && !CheckForExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, true))
    return {};

  return enabled_extensions;
}

bool CheckDebugLayerAvailability()
{
  uint32_t extension_count;
  VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
    return false;
  }

  std::vector<VkExtensionProperties> extension_list(extension_count);
  res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_list.data());
  assert(res == VK_SUCCESS);

  uint32_t layer_count;
  res = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
    return false;
  }

  std::vector<VkLayerProperties> layer_list(layer_count);
  res = vkEnumerateInstanceLayerProperties(&layer_count, layer_list.data());
  assert(res == VK_SUCCESS);

  // Check for both VK_EXT_debug_report and VK_LAYER_LUNARG_standard_validation
  return (std::find_if(extension_list.begin(), extension_list.end(),
                       [](const auto& it) {
                         return (strcmp(it.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0);
                       }) != extension_list.end() &&
          std::find_if(layer_list.begin(), layer_list.end(), [](const auto& it) {
            return (strcmp(it.layerName, "VK_LAYER_LUNARG_standard_validation") == 0);
          }) != layer_list.end());
}

std::vector<VkPhysicalDevice> EnumerateVulkanPhysicalDevices(VkInstance instance)
{
  uint32_t physicalDeviceCount;
  VkResult res = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices failed: ");
    return {};
  }

  std::vector<VkPhysicalDevice> devices;
  devices.resize(physicalDeviceCount);

  res = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, devices.data());
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices failed: ");
    return {};
  }

  return devices;
}

// At least one extension (swapchain) is required, so an empty vector returned by this function is
// an error case.
std::vector<const char*> SelectVulkanDeviceExtensions(VkPhysicalDevice physical_device)
{
  uint32_t extension_count;
  VkResult res =
      vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateDeviceExtensionProperties failed: ");
    return {};
  }

  if (extension_count == 0)
  {
    ERROR_LOG(VIDEO, "Vulkan: No extensions supported by device.");
    return {};
  }

  std::vector<VkExtensionProperties> extension_list(extension_count);
  res = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count,
                                             extension_list.data());
  assert(res == VK_SUCCESS);

  for (const auto& extension_properties : extension_list)
    INFO_LOG(VIDEO, "Available extension: %s", extension_properties.extensionName);

  std::vector<const char*> enabled_extensions;
  auto CheckForExtension = [&](const char* name, bool required) -> bool {
    if (std::find_if(extension_list.begin(), extension_list.end(),
                     [&](const VkExtensionProperties& properties) {
                       return !strcmp(name, properties.extensionName);
                     }) != extension_list.end())
    {
      INFO_LOG(VIDEO, "Enabling extension: %s", name);
      enabled_extensions.push_back(name);
      return true;
    }

    if (required)
    {
      ERROR_LOG(VIDEO, "Vulkan: Missing required extension %s.", name);
      return false;
    }

    return true;
  };

  if (!CheckForExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME, true))
  {
    return {};
  }

  return enabled_extensions;
}

bool SelectVulkanDeviceFeatures(VkPhysicalDevice device, VkPhysicalDeviceFeatures* enable_features)
{
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(device, &properties);

  VkPhysicalDeviceFeatures available_features;
  vkGetPhysicalDeviceFeatures(device, &available_features);

  *enable_features = {};

  // Not having geometry shaders or wide lines will cause issues with rendering.
  if (!available_features.geometryShader && !available_features.wideLines)
    WARN_LOG(VIDEO, "Vulkan: Missing both geometryShader and wideLines features.");
  if (!available_features.largePoints)
    WARN_LOG(VIDEO, "Vulkan: Missing large points feature. CPU EFB writes will be slower.");

  // Enable the features we use.
  enable_features->dualSrcBlend = available_features.dualSrcBlend;
  enable_features->geometryShader = available_features.geometryShader;
  enable_features->samplerAnisotropy = available_features.samplerAnisotropy;
  enable_features->logicOp = available_features.logicOp;
  enable_features->fragmentStoresAndAtomics = available_features.fragmentStoresAndAtomics;
  enable_features->sampleRateShading = available_features.sampleRateShading;
  enable_features->largePoints = available_features.largePoints;

  // Only here to shut up the debug layer, we don't actually use it
  enable_features->shaderClipDistance = available_features.shaderClipDistance;
  enable_features->shaderCullDistance = available_features.shaderCullDistance;

  // Dual source blending seems to be broken on AMD drivers (At least as of 16.5.1).
  // Creating a pipeline with a fragment shader that has an output with the Index
  // decoration causes it to fail silently.
  if (properties.vendorID == 0x1002)
    enable_features->dualSrcBlend = VK_FALSE;

  return true;
}

void PopulateBackendInfo(VideoConfig* config)
{
  config->backend_info.APIType = API_VULKAN;
  config->backend_info.bSupportsExclusiveFullscreen = false;  // Does WSI even allow this?
  config->backend_info.bSupports3DVision = false;             // Does WSI even allow this?
  config->backend_info.bSupportsOversizedViewports = true;    // TODO: Check spec for this one.
  config->backend_info.bSupportsEarlyZ = true;                // Assumed support?
  config->backend_info.bSupportsPrimitiveRestart = true;      // Assumed support.
  config->backend_info.bSupportsBindingLayout = false;        // Assumed support.
  config->backend_info.bSupportsPaletteConversion = true;     // Assumed support.
  config->backend_info.bSupportsClipControl = true;           // Assumed support.
  config->backend_info.bSupportsMultithreading = true;        // Assumed support.
  config->backend_info.bSupportsPostProcessing = false;       // No support yet.
  config->backend_info.bSupportsDualSourceBlend = false;      // Dependant on features.
  config->backend_info.bSupportsGeometryShaders = false;      // Dependant on features.
  config->backend_info.bSupportsGSInstancing = false;         // Dependant on features.
  config->backend_info.bSupportsBBox = false;                 // Dependant on features.
  config->backend_info.bSupportsSSAA = false;                 // Dependant on features.
}

void PopulateBackendInfoAdapters(VideoConfig* config,
                                 const std::vector<VkPhysicalDevice>& physical_device_list)
{
  for (VkPhysicalDevice physical_device : physical_device_list)
  {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    config->backend_info.Adapters.push_back(properties.deviceName);
  }
}

void PopulateBackendInfoFeatures(VideoConfig* config, VkPhysicalDevice physical_device,
                                 const VkPhysicalDeviceFeatures& features)
{
  config->backend_info.bSupportsDualSourceBlend = (features.dualSrcBlend == VK_TRUE);
  config->backend_info.bSupportsGeometryShaders = (features.geometryShader == VK_TRUE);
  config->backend_info.bSupportsGSInstancing = (features.geometryShader == VK_TRUE);
  config->backend_info.bSupportsBBox = (features.fragmentStoresAndAtomics == VK_TRUE);
  config->backend_info.bSupportsSSAA = (features.sampleRateShading == VK_TRUE);
}

void PopulateBackendInfoMultisampleModes(VideoConfig* config, VkPhysicalDevice physical_device,
                                         const VkPhysicalDeviceProperties& properties)
{
  // Query image support for the EFB texture formats.
  VkImageFormatProperties efb_color_properties = {};
  vkGetPhysicalDeviceImageFormatProperties(
      physical_device, EFB_COLOR_TEXTURE_FORMAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &efb_color_properties);
  VkImageFormatProperties efb_depth_properties = {};
  vkGetPhysicalDeviceImageFormatProperties(
      physical_device, EFB_DEPTH_TEXTURE_FORMAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &efb_depth_properties);

  // We can only support MSAA if it's supported on our render target formats.
  VkSampleCountFlags supported_sample_counts = properties.limits.framebufferColorSampleCounts &
                                               properties.limits.framebufferDepthSampleCounts &
                                               efb_color_properties.sampleCounts &
                                               efb_color_properties.sampleCounts;

  // No AA
  config->backend_info.AAModes.clear();
  config->backend_info.AAModes.emplace_back(1);

  // 2xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_2_BIT)
    config->backend_info.AAModes.emplace_back(2);

  // 4xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_4_BIT)
    config->backend_info.AAModes.emplace_back(4);

  // 8xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_8_BIT)
    config->backend_info.AAModes.emplace_back(8);

  // 16xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_16_BIT)
    config->backend_info.AAModes.emplace_back(16);

  // 32xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_32_BIT)
    config->backend_info.AAModes.emplace_back(32);

  // 64xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_64_BIT)
    config->backend_info.AAModes.emplace_back(64);
}

VkSurfaceKHR CreateVulkanSurface(VkInstance instance, void* hwnd)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)

  VkWin32SurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // VkStructureType               sType
      nullptr,                                          // const void*                   pNext
      0,                                                // VkWin32SurfaceCreateFlagsKHR  flags
      nullptr,                                          // HINSTANCE                     hinstance
      reinterpret_cast<HWND>(hwnd)                      // HWND                          hwnd
  };

  VkSurfaceKHR surface;
  VkResult res = vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateWin32SurfaceKHR failed: ");
    return nullptr;
  }

  return surface;

#elif defined(VK_USE_PLATFORM_XLIB_KHR)

  // This doesn't seem right, but matches what GLX does.
  Display* display = XOpenDisplay(nullptr);

  VkXlibSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,  // VkStructureType               sType
      nullptr,                                         // const void*                   pNext
      0,                                               // VkXlibSurfaceCreateFlagsKHR   flags
      display,                                         // Display*                      dpy
      reinterpret_cast<Window>(hwnd)                   // Window                        window
  };

  VkSurfaceKHR surface;
  VkResult res = vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateXlibSurfaceKHR failed: ");
    return nullptr;
  }

  return surface;

#elif defined(VK_USE_PLATFORM_XCB_KHR)

  // This doesn't seem right, but matches what GLX does.
  Display* display = XOpenDisplay(nullptr);
  xcb_connection_t* connection = XGetXCBConnection(display);

  VkXcbSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,  // VkStructureType               sType
      nullptr,                                        // const void*                   pNext
      0,                                              // VkXcbSurfaceCreateFlagsKHR    flags
      connection,                                     // xcb_connection_t*             connection
      static_cast<xcb_window_t>(reinterpret_cast<uintptr_t>(hwnd))  // xcb_window_t window
  };

  VkSurfaceKHR surface;
  VkResult res = vkCreateXcbSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateXcbSurfaceKHR failed: ");
    return nullptr;
  }

  return surface;

#elif defined(VK_USE_PLATFORM_ANDROID_KHR)

  VkAndroidSurfaceCreateInfoKHR surface_create_info = {
      VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,  // VkStructureType                sType
      nullptr,                                            // const void*                    pNext
      0,                                                  // VkAndroidSurfaceCreateFlagsKHR flags
      reinterpret_cast<ANativeWindow*>(hwnd)              // ANativeWindow*                 window
  };

  VkSurfaceKHR surface;
  VkResult res = vkCreateAndroidSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateAndroidSurfaceKHR failed: ");
    return nullptr;
  }

  return surface;

#else
  return nullptr;
#endif
}

VkDevice CreateVulkanDevice(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                            uint32_t* out_graphics_queue_family_index, VkQueue* out_graphics_queue,
                            uint32_t* out_present_queue_family_index, VkQueue* out_present_queue,
                            VkQueueFamilyProperties* out_graphics_queue_family_properties,
                            const VkPhysicalDeviceFeatures& features, bool enable_debug_layer)
{
  uint32_t queue_family_count;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
  if (queue_family_count == 0)
  {
    ERROR_LOG(VIDEO, "No queue families found on specified vulkan physical device.");
    return nullptr;
  }

  std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                           queue_family_properties.data());
  INFO_LOG(VIDEO, "%u vulkan queue families", queue_family_count);

  // Find a graphics queue
  uint32_t graphics_queue_family_index = queue_family_count;
  for (uint32_t i = 0; i < queue_family_count; i++)
  {
    if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      // Check that it can present to our surface from this queue
      VkBool32 presentSupported;
      VkResult res =
          vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &presentSupported);
      if (res != VK_SUCCESS)
      {
        LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceSupportKHR failed: ");
        return nullptr;
      }

      if (presentSupported)
      {
        graphics_queue_family_index = i;
        break;
      }
    }
  }
  if (graphics_queue_family_index == queue_family_count)
  {
    ERROR_LOG(VIDEO, "Vulkan: Failed to find an acceptable graphics queue.");
    return nullptr;
  }

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = nullptr;
  device_info.flags = 0;

  static constexpr float queue_priorities[] = {1.0f};
  VkDeviceQueueCreateInfo queue_info = {};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.pNext = nullptr;
  queue_info.flags = 0;
  queue_info.queueFamilyIndex = graphics_queue_family_index;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = queue_priorities;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  // assert(graphics_queue_family_index != present_queue_family_index);

  std::vector<const char*> enabled_extensions = SelectVulkanDeviceExtensions(physical_device);
  if (enabled_extensions.empty())
    return nullptr;

  device_info.enabledLayerCount = 0;
  device_info.ppEnabledLayerNames = nullptr;
  device_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
  device_info.ppEnabledExtensionNames = enabled_extensions.data();
  device_info.pEnabledFeatures = &features;

  // Enable debug layer on debug builds
  if (enable_debug_layer)
  {
    static const char* layer_names[] = {"VK_LAYER_LUNARG_standard_validation"};
    device_info.enabledLayerCount = 1;
    device_info.ppEnabledLayerNames = layer_names;
  }

  VkDevice device;
  VkResult res = vkCreateDevice(physical_device, &device_info, nullptr, &device);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateDevice failed: ");
    return nullptr;
  }

  *out_graphics_queue_family_index = graphics_queue_family_index;
  *out_graphics_queue_family_properties = queue_family_properties[graphics_queue_family_index];
  vkGetDeviceQueue(device, graphics_queue_family_index, 0, out_graphics_queue);
  *out_present_queue_family_index = graphics_queue_family_index;
  vkGetDeviceQueue(device, graphics_queue_family_index, 0, out_present_queue);
  return device;
}

VkPresentModeKHR SelectVulkanPresentMode(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
  VkResult res;
  uint32_t mode_count;
  res = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, nullptr);
  if (res != VK_SUCCESS || mode_count == 0)
  {
    LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceFormatsKHR failed: ");
    return VK_PRESENT_MODE_RANGE_SIZE_KHR;
  }

  std::vector<VkPresentModeKHR> present_modes(mode_count);
  res = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count,
                                                  present_modes.data());
  assert(res == VK_SUCCESS);

  // Checks if a particular mode is supported, if it is, returns that mode.
  auto CheckForMode = [&present_modes](VkPresentModeKHR check_mode) {
    auto it = std::find_if(present_modes.begin(), present_modes.end(),
                           [check_mode](VkPresentModeKHR mode) { return (check_mode == mode); });
    return (it != present_modes.end());
  };

  // If vsync is enabled, prefer VK_PRESENT_MODE_FIFO_KHR.
  if (g_ActiveConfig.IsVSync())
  {
    // Try for relaxed vsync first, since it's likely the VI won't line up with
    // the refresh rate of the system exactly, so tearing once is better than
    // waiting for the next vblank.
    if (CheckForMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR))
      return VK_PRESENT_MODE_FIFO_RELAXED_KHR;

    // Fall back to strict vsync.
    if (CheckForMode(VK_PRESENT_MODE_FIFO_KHR))
    {
      WARN_LOG(VIDEO, "Vulkan: FIFO_RELAXED not available, falling back to FIFO.");
      return VK_PRESENT_MODE_FIFO_KHR;
    }
  }

  // Prefer screen-tearing, if possible, for lowest latency.
  if (CheckForMode(VK_PRESENT_MODE_IMMEDIATE_KHR))
    return VK_PRESENT_MODE_IMMEDIATE_KHR;

  // Use optimized-vsync above vsync.
  if (CheckForMode(VK_PRESENT_MODE_MAILBOX_KHR))
    return VK_PRESENT_MODE_MAILBOX_KHR;

  // Fall back to whatever is available.
  return present_modes[0];
}

VkSurfaceFormatKHR SelectVulkanSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
  uint32_t format_count;
  VkResult res =
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
  if (res != VK_SUCCESS || format_count == 0)
  {
    LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceFormatsKHR failed: ");
    return {VK_FORMAT_RANGE_SIZE, VK_COLOR_SPACE_RANGE_SIZE_KHR};
  }

  std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
  res = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                             surface_formats.data());
  assert(res == VK_SUCCESS);

  // If there is a single undefined surface format, the device doesn't care, so we'll just use RGBA
  if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
    return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  // Return the first surface format, just use what it prefers.
  VkSurfaceFormatKHR result = surface_formats[0];

  // Some drivers seem to return a SRGB format here (Intel Mesa).
  // This results in gamma correction when presenting to the screen, which we don't want.
  // Use a linear format instead, if this is the case.
  result.format = GetLinearFormat(result.format);
  return result;
}

bool IsDepthFormat(VkFormat format)
{
  switch (format)
  {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

VkFormat GetLinearFormat(VkFormat format)
{
  switch (format)
  {
  case VK_FORMAT_R8_SRGB:
    return VK_FORMAT_R8_UNORM;
  case VK_FORMAT_R8G8_SRGB:
    return VK_FORMAT_R8G8_UNORM;
  case VK_FORMAT_R8G8B8_SRGB:
    return VK_FORMAT_R8G8B8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VK_FORMAT_B8G8R8_SRGB:
    return VK_FORMAT_B8G8R8_UNORM;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return VK_FORMAT_B8G8R8A8_UNORM;
  default:
    return format;
  }
}

u32 GetTexelSize(VkFormat format)
{
  // Only contains pixel formats we use.
  switch (format)
  {
  case VK_FORMAT_R32_SFLOAT:
    return 4;

  case VK_FORMAT_D32_SFLOAT:
    return 4;

  case VK_FORMAT_R8G8B8A8_UNORM:
    return 4;

  default:
    PanicAlert("Unhandled pixel format");
    return 1;
  }
}

template <>
DeferredResourceDestruction
DeferredResourceDestruction::Wrapper<VkCommandPool>(VkCommandPool object)
{
  DeferredResourceDestruction ret;
  ret.object.CommandPool = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyCommandPool(device, obj.CommandPool, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction
DeferredResourceDestruction::Wrapper<VkDeviceMemory>(VkDeviceMemory object)
{
  DeferredResourceDestruction ret;
  ret.object.DeviceMemory = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkFreeMemory(device, obj.DeviceMemory, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction DeferredResourceDestruction::Wrapper<VkBuffer>(VkBuffer object)
{
  DeferredResourceDestruction ret;
  ret.object.Buffer = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyBuffer(device, obj.Buffer, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction DeferredResourceDestruction::Wrapper<VkBufferView>(VkBufferView object)
{
  DeferredResourceDestruction ret;
  ret.object.BufferView = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyBufferView(device, obj.BufferView, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction DeferredResourceDestruction::Wrapper<VkImage>(VkImage object)
{
  DeferredResourceDestruction ret;
  ret.object.Image = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyImage(device, obj.Image, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction DeferredResourceDestruction::Wrapper<VkImageView>(VkImageView object)
{
  DeferredResourceDestruction ret;
  ret.object.ImageView = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyImageView(device, obj.ImageView, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction DeferredResourceDestruction::Wrapper<VkRenderPass>(VkRenderPass object)
{
  DeferredResourceDestruction ret;
  ret.object.RenderPass = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyRenderPass(device, obj.RenderPass, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction
DeferredResourceDestruction::Wrapper<VkFramebuffer>(VkFramebuffer object)
{
  DeferredResourceDestruction ret;
  ret.object.Framebuffer = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyFramebuffer(device, obj.Framebuffer, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction
DeferredResourceDestruction::Wrapper<VkSwapchainKHR>(VkSwapchainKHR object)
{
  DeferredResourceDestruction ret;
  ret.object.Swapchain = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroySwapchainKHR(device, obj.Swapchain, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction
DeferredResourceDestruction::Wrapper<VkShaderModule>(VkShaderModule object)
{
  DeferredResourceDestruction ret;
  ret.object.ShaderModule = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyShaderModule(device, obj.ShaderModule, nullptr);
  };
  return ret;
}

template <>
DeferredResourceDestruction DeferredResourceDestruction::Wrapper<VkPipeline>(VkPipeline object)
{
  DeferredResourceDestruction ret;
  ret.object.Pipeline = object;
  ret.destroy_callback = [](VkDevice device, const Object& obj) {
    vkDestroyPipeline(device, obj.Pipeline, nullptr);
  };
  return ret;
}

}  // namespace Vulkan
