// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/VideoCommon.h"

namespace Vulkan
{
std::unique_ptr<VulkanContext> g_vulkan_context;

VulkanContext::VulkanContext(VkInstance instance, VkPhysicalDevice physical_device,
                             VkSurfaceKHR surface)
    : m_instance(instance), m_physical_device(physical_device), m_surface(surface)
{
  // Read device physical memory properties, we need it for allocating buffers
  vkGetPhysicalDeviceProperties(physical_device, &m_device_properties);
  vkGetPhysicalDeviceMemoryProperties(physical_device, &m_device_memory_properties);

  // Would any drivers be this silly? I hope not...
  m_device_properties.limits.minUniformBufferOffsetAlignment = std::max(
      m_device_properties.limits.minUniformBufferOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_properties.limits.minTexelBufferOffsetAlignment = std::max(
      m_device_properties.limits.minTexelBufferOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_properties.limits.optimalBufferCopyOffsetAlignment = std::max(
      m_device_properties.limits.optimalBufferCopyOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_properties.limits.optimalBufferCopyRowPitchAlignment = std::max(
      m_device_properties.limits.optimalBufferCopyRowPitchAlignment, static_cast<VkDeviceSize>(1));
}

VulkanContext::~VulkanContext()
{
  if (m_device != VK_NULL_HANDLE)
    vkDestroyDevice(m_device, nullptr);

  if (m_surface != VK_NULL_HANDLE)
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

  if (m_debug_report_callback != VK_NULL_HANDLE)
    DisableDebugReports();

  vkDestroyInstance(m_instance, nullptr);
}

bool VulkanContext::CheckValidationLayerAvailablility()
{
  u32 extension_count = 0;
  VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
    return false;
  }

  std::vector<VkExtensionProperties> extension_list(extension_count);
  res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_list.data());
  ASSERT(res == VK_SUCCESS);

  u32 layer_count = 0;
  res = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
    return false;
  }

  std::vector<VkLayerProperties> layer_list(layer_count);
  res = vkEnumerateInstanceLayerProperties(&layer_count, layer_list.data());
  ASSERT(res == VK_SUCCESS);

  // Check for both VK_EXT_debug_report and VK_LAYER_LUNARG_standard_validation
  return (std::find_if(extension_list.begin(), extension_list.end(),
                       [](const auto& it) {
                         return strcmp(it.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0;
                       }) != extension_list.end() &&
          std::find_if(layer_list.begin(), layer_list.end(), [](const auto& it) {
            return strcmp(it.layerName, "VK_LAYER_LUNARG_standard_validation") == 0;
          }) != layer_list.end());
}

VkInstance VulkanContext::CreateVulkanInstance(WindowSystemType wstype, bool enable_debug_report,
                                               bool enable_validation_layer)
{
  std::vector<const char*> enabled_extensions;
  if (!SelectInstanceExtensions(&enabled_extensions, wstype, enable_debug_report))
    return VK_NULL_HANDLE;

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = nullptr;
  app_info.pApplicationName = "Dolphin Emulator";
  app_info.applicationVersion = VK_MAKE_VERSION(5, 0, 0);
  app_info.pEngineName = "Dolphin Emulator";
  app_info.engineVersion = VK_MAKE_VERSION(5, 0, 0);
  app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  // Try for Vulkan 1.1 if the loader supports it.
  if (vkEnumerateInstanceVersion)
  {
    u32 supported_api_version = 0;
    VkResult res = vkEnumerateInstanceVersion(&supported_api_version);
    if (res == VK_SUCCESS && (VK_VERSION_MAJOR(supported_api_version) > 1 ||
                              VK_VERSION_MINOR(supported_api_version) >= 1))
    {
      // The device itself may not support 1.1, so we check that before using any 1.1 functionality.
      app_info.apiVersion = VK_MAKE_VERSION(1, 1, 0);
    }
  }

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
  if (enable_validation_layer)
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

bool VulkanContext::SelectInstanceExtensions(std::vector<const char*>* extension_list,
                                             WindowSystemType wstype, bool enable_debug_report)
{
  u32 extension_count = 0;
  VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
    return false;
  }

  if (extension_count == 0)
  {
    ERROR_LOG(VIDEO, "Vulkan: No extensions supported by instance.");
    return false;
  }

  std::vector<VkExtensionProperties> available_extension_list(extension_count);
  res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                               available_extension_list.data());
  ASSERT(res == VK_SUCCESS);

  for (const auto& extension_properties : available_extension_list)
    INFO_LOG(VIDEO, "Available extension: %s", extension_properties.extensionName);

  auto AddExtension = [&](const char* name, bool required) {
    if (std::find_if(available_extension_list.begin(), available_extension_list.end(),
                     [&](const VkExtensionProperties& properties) {
                       return !strcmp(name, properties.extensionName);
                     }) != available_extension_list.end())
    {
      INFO_LOG(VIDEO, "Enabling extension: %s", name);
      extension_list->push_back(name);
      return true;
    }

    if (required)
      ERROR_LOG(VIDEO, "Vulkan: Missing required extension %s.", name);

    return false;
  };

  // Common extensions
  if (wstype != WindowSystemType::Headless && !AddExtension(VK_KHR_SURFACE_EXTENSION_NAME, true))
  {
    return false;
  }

#if defined(VK_USE_PLATFORM_WIN32_KHR)
  if (wstype == WindowSystemType::Windows &&
      !AddExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true))
  {
    return false;
  }
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
  if (wstype == WindowSystemType::X11 && !AddExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME, true))
  {
    return false;
  }
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  if (wstype == WindowSystemType::Android &&
      !AddExtension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, true))
  {
    return false;
  }
#endif
#if defined(VK_USE_PLATFORM_METAL_EXT)
  if (wstype == WindowSystemType::MacOS && !AddExtension(VK_EXT_METAL_SURFACE_EXTENSION_NAME, true))
  {
    return false;
  }
#endif

  // VK_EXT_debug_report
  if (enable_debug_report && !AddExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, false))
    WARN_LOG(VIDEO, "Vulkan: Debug report requested, but extension is not available.");

  AddExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, false);
  AddExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, false);

  return true;
}

VulkanContext::GPUList VulkanContext::EnumerateGPUs(VkInstance instance)
{
  u32 gpu_count = 0;
  VkResult res = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices failed: ");
    return {};
  }

  GPUList gpus;
  gpus.resize(gpu_count);

  res = vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices failed: ");
    return {};
  }

  return gpus;
}

void VulkanContext::PopulateBackendInfo(VideoConfig* config)
{
  config->backend_info.api_type = APIType::Vulkan;
  config->backend_info.bSupports3DVision = false;                  // D3D-exclusive.
  config->backend_info.bSupportsOversizedViewports = true;         // Assumed support.
  config->backend_info.bSupportsEarlyZ = true;                     // Assumed support.
  config->backend_info.bSupportsPrimitiveRestart = true;           // Assumed support.
  config->backend_info.bSupportsBindingLayout = false;             // Assumed support.
  config->backend_info.bSupportsPaletteConversion = true;          // Assumed support.
  config->backend_info.bSupportsClipControl = true;                // Assumed support.
  config->backend_info.bSupportsMultithreading = true;             // Assumed support.
  config->backend_info.bSupportsComputeShaders = true;             // Assumed support.
  config->backend_info.bSupportsGPUTextureDecoding = true;         // Assumed support.
  config->backend_info.bSupportsBitfield = true;                   // Assumed support.
  config->backend_info.bSupportsPartialDepthCopies = true;         // Assumed support.
  config->backend_info.bSupportsShaderBinaries = true;             // Assumed support.
  config->backend_info.bSupportsPipelineCacheData = false;         // Handled via pipeline caches.
  config->backend_info.bSupportsDynamicSamplerIndexing = true;     // Assumed support.
  config->backend_info.bSupportsPostProcessing = true;             // Assumed support.
  config->backend_info.bSupportsBackgroundCompiling = true;        // Assumed support.
  config->backend_info.bSupportsCopyToVram = true;                 // Assumed support.
  config->backend_info.bSupportsReversedDepthRange = true;         // Assumed support.
  config->backend_info.bSupportsExclusiveFullscreen = false;       // Dependent on OS and features.
  config->backend_info.bSupportsDualSourceBlend = false;           // Dependent on features.
  config->backend_info.bSupportsGeometryShaders = false;           // Dependent on features.
  config->backend_info.bSupportsGSInstancing = false;              // Dependent on features.
  config->backend_info.bSupportsBBox = false;                      // Dependent on features.
  config->backend_info.bSupportsFragmentStoresAndAtomics = false;  // Dependent on features.
  config->backend_info.bSupportsSSAA = false;                      // Dependent on features.
  config->backend_info.bSupportsDepthClamp = false;                // Dependent on features.
  config->backend_info.bSupportsST3CTextures = false;              // Dependent on features.
  config->backend_info.bSupportsBPTCTextures = false;              // Dependent on features.
  config->backend_info.bSupportsLogicOp = false;                   // Dependent on features.
  config->backend_info.bSupportsLargePoints = false;               // Dependent on features.
  config->backend_info.bSupportsFramebufferFetch = false;          // No support.
}

void VulkanContext::PopulateBackendInfoAdapters(VideoConfig* config, const GPUList& gpu_list)
{
  config->backend_info.Adapters.clear();
  for (VkPhysicalDevice physical_device : gpu_list)
  {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    config->backend_info.Adapters.push_back(properties.deviceName);
  }
}

void VulkanContext::PopulateBackendInfoFeatures(VideoConfig* config, VkPhysicalDevice gpu,
                                                const VkPhysicalDeviceProperties& properties,
                                                const VkPhysicalDeviceFeatures& features)
{
  config->backend_info.MaxTextureSize = properties.limits.maxImageDimension2D;
  config->backend_info.bUsesLowerLeftOrigin = false;
  config->backend_info.bSupportsDualSourceBlend = (features.dualSrcBlend == VK_TRUE);
  config->backend_info.bSupportsGeometryShaders = (features.geometryShader == VK_TRUE);
  config->backend_info.bSupportsGSInstancing = (features.geometryShader == VK_TRUE);
  config->backend_info.bSupportsBBox = config->backend_info.bSupportsFragmentStoresAndAtomics =
      (features.fragmentStoresAndAtomics == VK_TRUE);
  config->backend_info.bSupportsSSAA = (features.sampleRateShading == VK_TRUE);
  config->backend_info.bSupportsLogicOp = (features.logicOp == VK_TRUE);

  // Disable geometry shader when shaderTessellationAndGeometryPointSize is not supported.
  // Seems this is needed for gl_Layer.
  if (!features.shaderTessellationAndGeometryPointSize)
  {
    config->backend_info.bSupportsGeometryShaders = VK_FALSE;
    config->backend_info.bSupportsGSInstancing = VK_FALSE;
  }

  // Depth clamping implies shaderClipDistance and depthClamp
  config->backend_info.bSupportsDepthClamp =
      (features.depthClamp == VK_TRUE && features.shaderClipDistance == VK_TRUE);

  // textureCompressionBC implies BC1 through BC7, which is a superset of DXT1/3/5, which we need.
  const bool supports_bc = features.textureCompressionBC == VK_TRUE;
  config->backend_info.bSupportsST3CTextures = supports_bc;
  config->backend_info.bSupportsBPTCTextures = supports_bc;

  // Some devices don't support point sizes >1 (e.g. Adreno).
  // If we can't use a point size above our maximum IR, use triangles instead for EFB pokes.
  // This means a 6x increase in the size of the vertices, though.
  config->backend_info.bSupportsLargePoints = features.largePoints &&
                                              properties.limits.pointSizeRange[0] <= 1.0f &&
                                              properties.limits.pointSizeRange[1] >= 16;

  // Our usage of primitive restart appears to be broken on AMD's binary drivers.
  // Seems to be fine on GCN Gen 1-2, unconfirmed on GCN Gen 3, causes driver resets on GCN Gen 4.
  if (DriverDetails::HasBug(DriverDetails::BUG_PRIMITIVE_RESTART))
    config->backend_info.bSupportsPrimitiveRestart = false;

  // Reversed depth range is broken on some drivers, or is broken when used in combination
  // with depth clamping. Fall back to inverted depth range for these.
  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_REVERSED_DEPTH_RANGE))
    config->backend_info.bSupportsReversedDepthRange = false;
}

void VulkanContext::PopulateBackendInfoMultisampleModes(
    VideoConfig* config, VkPhysicalDevice gpu, const VkPhysicalDeviceProperties& properties)
{
  // Query image support for the EFB texture formats.
  VkImageFormatProperties efb_color_properties = {};
  vkGetPhysicalDeviceImageFormatProperties(
      gpu, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &efb_color_properties);
  VkImageFormatProperties efb_depth_properties = {};
  vkGetPhysicalDeviceImageFormatProperties(
      gpu, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &efb_depth_properties);

  // We can only support MSAA if it's supported on our render target formats.
  VkSampleCountFlags supported_sample_counts = properties.limits.framebufferColorSampleCounts &
                                               properties.limits.framebufferDepthSampleCounts &
                                               efb_color_properties.sampleCounts &
                                               efb_depth_properties.sampleCounts;

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

std::unique_ptr<VulkanContext> VulkanContext::Create(VkInstance instance, VkPhysicalDevice gpu,
                                                     VkSurfaceKHR surface,
                                                     bool enable_debug_reports,
                                                     bool enable_validation_layer)
{
  std::unique_ptr<VulkanContext> context = std::make_unique<VulkanContext>(instance, gpu, surface);

  // Initialize DriverDetails so that we can check for bugs to disable features if needed.
  context->InitDriverDetails();
  context->PopulateShaderSubgroupSupport();

  // Enable debug reports if the "Host GPU" log category is enabled.
  if (enable_debug_reports)
    context->EnableDebugReports();

  // Attempt to create the device.
  if (!context->CreateDevice(enable_validation_layer))
    return nullptr;

  return context;
}

bool VulkanContext::SelectDeviceExtensions(bool enable_surface)
{
  u32 extension_count = 0;
  VkResult res =
      vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &extension_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateDeviceExtensionProperties failed: ");
    return false;
  }

  if (extension_count == 0)
  {
    ERROR_LOG(VIDEO, "Vulkan: No extensions supported by device.");
    return false;
  }

  std::vector<VkExtensionProperties> available_extension_list(extension_count);
  res = vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &extension_count,
                                             available_extension_list.data());
  ASSERT(res == VK_SUCCESS);

  for (const auto& extension_properties : available_extension_list)
    INFO_LOG(VIDEO, "Available extension: %s", extension_properties.extensionName);

  auto AddExtension = [&](const char* name, bool required) {
    if (std::find_if(available_extension_list.begin(), available_extension_list.end(),
                     [&](const VkExtensionProperties& properties) {
                       return !strcmp(name, properties.extensionName);
                     }) != available_extension_list.end())
    {
      INFO_LOG(VIDEO, "Enabling extension: %s", name);
      m_device_extensions.push_back(name);
      return true;
    }

    if (required)
      ERROR_LOG(VIDEO, "Vulkan: Missing required extension %s.", name);

    return false;
  };

  if (enable_surface && !AddExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME, true))
    return false;

#ifdef SUPPORTS_VULKAN_EXCLUSIVE_FULLSCREEN
  // VK_EXT_full_screen_exclusive
  if (AddExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME, true))
    INFO_LOG(VIDEO, "Using VK_EXT_full_screen_exclusive for exclusive fullscreen.");
#endif

  return true;
}

bool VulkanContext::SelectDeviceFeatures()
{
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(m_physical_device, &properties);

  VkPhysicalDeviceFeatures available_features;
  vkGetPhysicalDeviceFeatures(m_physical_device, &available_features);

  // Not having geometry shaders or wide lines will cause issues with rendering.
  if (!available_features.geometryShader && !available_features.wideLines)
    WARN_LOG(VIDEO, "Vulkan: Missing both geometryShader and wideLines features.");
  if (!available_features.largePoints)
    WARN_LOG(VIDEO, "Vulkan: Missing large points feature. CPU EFB writes will be slower.");
  if (!available_features.occlusionQueryPrecise)
    WARN_LOG(VIDEO, "Vulkan: Missing precise occlusion queries. Perf queries will be inaccurate.");

  // Enable the features we use.
  m_device_features.dualSrcBlend = available_features.dualSrcBlend;
  m_device_features.geometryShader = available_features.geometryShader;
  m_device_features.samplerAnisotropy = available_features.samplerAnisotropy;
  m_device_features.logicOp = available_features.logicOp;
  m_device_features.fragmentStoresAndAtomics = available_features.fragmentStoresAndAtomics;
  m_device_features.sampleRateShading = available_features.sampleRateShading;
  m_device_features.largePoints = available_features.largePoints;
  m_device_features.shaderStorageImageMultisample =
      available_features.shaderStorageImageMultisample;
  m_device_features.shaderTessellationAndGeometryPointSize =
      available_features.shaderTessellationAndGeometryPointSize;
  m_device_features.occlusionQueryPrecise = available_features.occlusionQueryPrecise;
  m_device_features.shaderClipDistance = available_features.shaderClipDistance;
  m_device_features.depthClamp = available_features.depthClamp;
  m_device_features.textureCompressionBC = available_features.textureCompressionBC;
  return true;
}

bool VulkanContext::CreateDevice(bool enable_validation_layer)
{
  u32 queue_family_count;
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
  if (queue_family_count == 0)
  {
    ERROR_LOG(VIDEO, "No queue families found on specified vulkan physical device.");
    return false;
  }

  std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count,
                                           queue_family_properties.data());
  INFO_LOG(VIDEO, "%u vulkan queue families", queue_family_count);

  // Find graphics and present queues.
  m_graphics_queue_family_index = queue_family_count;
  m_present_queue_family_index = queue_family_count;
  for (uint32_t i = 0; i < queue_family_count; i++)
  {
    VkBool32 graphics_supported = queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
    if (graphics_supported)
    {
      m_graphics_queue_family_index = i;
      // Quit now, no need for a present queue.
      if (!m_surface)
      {
        break;
      }
    }

    if (m_surface)
    {
      VkBool32 present_supported;
      VkResult res =
          vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, m_surface, &present_supported);
      if (res != VK_SUCCESS)
      {
        LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceSupportKHR failed: ");
        return false;
      }

      if (present_supported)
      {
        m_present_queue_family_index = i;
      }

      // Prefer one queue family index that does both graphics and present.
      if (graphics_supported && present_supported)
      {
        break;
      }
    }
  }
  if (m_graphics_queue_family_index == queue_family_count)
  {
    ERROR_LOG(VIDEO, "Vulkan: Failed to find an acceptable graphics queue.");
    return false;
  }
  if (m_surface && m_present_queue_family_index == queue_family_count)
  {
    ERROR_LOG(VIDEO, "Vulkan: Failed to find an acceptable present queue.");
    return false;
  }

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = nullptr;
  device_info.flags = 0;

  static constexpr float queue_priorities[] = {1.0f};
  VkDeviceQueueCreateInfo graphics_queue_info = {};
  graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  graphics_queue_info.pNext = nullptr;
  graphics_queue_info.flags = 0;
  graphics_queue_info.queueFamilyIndex = m_graphics_queue_family_index;
  graphics_queue_info.queueCount = 1;
  graphics_queue_info.pQueuePriorities = queue_priorities;

  VkDeviceQueueCreateInfo present_queue_info = {};
  present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  present_queue_info.pNext = nullptr;
  present_queue_info.flags = 0;
  present_queue_info.queueFamilyIndex = m_present_queue_family_index;
  present_queue_info.queueCount = 1;
  present_queue_info.pQueuePriorities = queue_priorities;

  std::array<VkDeviceQueueCreateInfo, 2> queue_infos = {{
      graphics_queue_info,
      present_queue_info,
  }};

  device_info.queueCreateInfoCount = 1;
  if (m_graphics_queue_family_index != m_present_queue_family_index)
  {
    device_info.queueCreateInfoCount = 2;
  }
  device_info.pQueueCreateInfos = queue_infos.data();

  if (!SelectDeviceExtensions(m_surface != VK_NULL_HANDLE))
    return false;

  // convert std::string list to a char pointer list which we can feed in
  std::vector<const char*> extension_name_pointers;
  for (const std::string& name : m_device_extensions)
    extension_name_pointers.push_back(name.c_str());

  device_info.enabledLayerCount = 0;
  device_info.ppEnabledLayerNames = nullptr;
  device_info.enabledExtensionCount = static_cast<uint32_t>(extension_name_pointers.size());
  device_info.ppEnabledExtensionNames = extension_name_pointers.data();

  // Check for required features before creating.
  if (!SelectDeviceFeatures())
    return false;

  device_info.pEnabledFeatures = &m_device_features;

  // Enable debug layer on debug builds
  if (enable_validation_layer)
  {
    static const char* layer_names[] = {"VK_LAYER_LUNARG_standard_validation"};
    device_info.enabledLayerCount = 1;
    device_info.ppEnabledLayerNames = layer_names;
  }

  VkResult res = vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateDevice failed: ");
    return false;
  }

  // With the device created, we can fill the remaining entry points.
  if (!LoadVulkanDeviceFunctions(m_device))
    return false;

  // Grab the graphics and present queues.
  vkGetDeviceQueue(m_device, m_graphics_queue_family_index, 0, &m_graphics_queue);
  if (m_surface)
  {
    vkGetDeviceQueue(m_device, m_present_queue_family_index, 0, &m_present_queue);
  }
  return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(VkDebugReportFlagsEXT flags,
                                                          VkDebugReportObjectTypeEXT objectType,
                                                          uint64_t object, size_t location,
                                                          int32_t messageCode,
                                                          const char* pLayerPrefix,
                                                          const char* pMessage, void* pUserData)
{
  std::string log_message =
      StringFromFormat("Vulkan debug report: (%s) %s", pLayerPrefix ? pLayerPrefix : "", pMessage);
  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    GENERIC_LOG(Common::Log::HOST_GPU, Common::Log::LERROR, "%s", log_message.c_str());
  else if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
    GENERIC_LOG(Common::Log::HOST_GPU, Common::Log::LWARNING, "%s", log_message.c_str());
  else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    GENERIC_LOG(Common::Log::HOST_GPU, Common::Log::LINFO, "%s", log_message.c_str());
  else
    GENERIC_LOG(Common::Log::HOST_GPU, Common::Log::LDEBUG, "%s", log_message.c_str());

  return VK_FALSE;
}

bool VulkanContext::EnableDebugReports()
{
  // Already enabled?
  if (m_debug_report_callback != VK_NULL_HANDLE)
    return true;

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
      DebugReportCallback, nullptr};

  VkResult res =
      vkCreateDebugReportCallbackEXT(m_instance, &callback_info, nullptr, &m_debug_report_callback);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateDebugReportCallbackEXT failed: ");
    return false;
  }

  return true;
}

void VulkanContext::DisableDebugReports()
{
  if (m_debug_report_callback != VK_NULL_HANDLE)
  {
    vkDestroyDebugReportCallbackEXT(m_instance, m_debug_report_callback, nullptr);
    m_debug_report_callback = VK_NULL_HANDLE;
  }
}

std::optional<u32> VulkanContext::GetMemoryType(u32 bits, VkMemoryPropertyFlags properties,
                                                bool strict, bool* is_coherent)
{
  static constexpr u32 ALL_MEMORY_PROPERTY_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

  const u32 mask = strict ? ALL_MEMORY_PROPERTY_FLAGS : properties;

  for (u32 i = 0; i < VK_MAX_MEMORY_TYPES; i++)
  {
    if ((bits & (1 << i)) != 0)
    {
      const VkMemoryPropertyFlags type_flags =
          m_device_memory_properties.memoryTypes[i].propertyFlags;
      const VkMemoryPropertyFlags supported = type_flags & mask;
      if (supported == properties)
      {
        if (is_coherent)
          *is_coherent = (type_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        return i;
      }
    }
  }

  return std::nullopt;
}

u32 VulkanContext::GetUploadMemoryType(u32 bits, bool* is_coherent)
{
  static constexpr VkMemoryPropertyFlags COHERENT_FLAGS =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  // Try for coherent memory. Some drivers (looking at you, Adreno) have the cached type before the
  // uncached type, so use a strict check first.
  std::optional<u32> type_index = GetMemoryType(bits, COHERENT_FLAGS, true, is_coherent);
  if (type_index)
    return type_index.value();

  // Try for coherent memory, with any other bits set.
  type_index = GetMemoryType(bits, COHERENT_FLAGS, false, is_coherent);
  if (type_index)
  {
    WARN_LOG(VIDEO,
             "Strict check for upload memory properties failed, this may affect performance");
    return type_index.value();
  }

  // Fall back to non-coherent memory.
  WARN_LOG(
      VIDEO,
      "Vulkan: Failed to find a coherent memory type for uploads, this will affect performance.");
  type_index = GetMemoryType(bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false, is_coherent);
  if (type_index)
    return type_index.value();

  // Shouldn't happen, there should be at least one host-visible heap.
  PanicAlert("Unable to get memory type for upload.");
  return 0;
}

u32 VulkanContext::GetReadbackMemoryType(u32 bits, bool* is_coherent)
{
  std::optional<u32> type_index;

  // Mali driver appears to be significantly slower for readbacks when using cached memory.
  if (DriverDetails::HasBug(DriverDetails::BUG_SLOW_CACHED_READBACK_MEMORY))
  {
    type_index = GetMemoryType(
        bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true,
        is_coherent);
    if (type_index)
      return type_index.value();
  }

  // Optimal config uses cached+coherent.
  type_index =
      GetMemoryType(bits,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    true, is_coherent);
  if (type_index)
    return type_index.value();

  // Otherwise, prefer cached over coherent if we must choose one.
  type_index =
      GetMemoryType(bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                    false, is_coherent);
  if (type_index)
    return type_index.value();

  WARN_LOG(VIDEO, "Vulkan: Failed to find a cached memory type for readbacks, this will affect "
                  "performance.");
  type_index = GetMemoryType(bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false, is_coherent);
  *is_coherent = false;
  if (type_index)
    return type_index.value();

  // We should have at least one host visible memory type...
  PanicAlert("Unable to get memory type for upload.");
  return 0;
}

bool VulkanContext::SupportsDeviceExtension(const char* name) const
{
  return std::any_of(m_device_extensions.begin(), m_device_extensions.end(),
                     [name](const std::string& extension) { return extension == name; });
}

void VulkanContext::InitDriverDetails()
{
  DriverDetails::Vendor vendor;
  DriverDetails::Driver driver;

  // String comparisons aren't ideal, but there doesn't seem to be any other way to tell
  // which vendor a driver is for. These names are based on the reports submitted to
  // vulkan.gpuinfo.org, as of 19/09/2017.
  std::string device_name = m_device_properties.deviceName;
  u32 vendor_id = m_device_properties.vendorID;
  if (vendor_id == 0x10DE)
  {
    // Currently, there is only the official NV binary driver.
    // "NVIDIA" does not appear in the device name.
    vendor = DriverDetails::VENDOR_NVIDIA;
    driver = DriverDetails::DRIVER_NVIDIA;
  }
  else if (vendor_id == 0x1002 || vendor_id == 0x1022 ||
           device_name.find("AMD") != std::string::npos)
  {
    // RADV always advertises its name in the device string.
    // If not RADV, assume the AMD binary driver.
    if (device_name.find("RADV") != std::string::npos)
    {
      vendor = DriverDetails::VENDOR_MESA;
      driver = DriverDetails::DRIVER_R600;
    }
    else
    {
      vendor = DriverDetails::VENDOR_ATI;
      driver = DriverDetails::DRIVER_ATI;
    }
  }
  else if (vendor_id == 0x8086 || vendor_id == 0x8087 ||
           device_name.find("Intel") != std::string::npos)
  {
// Apart from the driver version, Intel does not appear to provide a way to
// differentiate between anv and the binary driver (Skylake+). Assume to be
// using anv if we not running on Windows.
#ifdef WIN32
    vendor = DriverDetails::VENDOR_INTEL;
    driver = DriverDetails::DRIVER_INTEL;
#else
    vendor = DriverDetails::VENDOR_MESA;
    driver = DriverDetails::DRIVER_I965;
#endif
  }
  else if (vendor_id == 0x5143 || device_name.find("Adreno") != std::string::npos)
  {
    // Currently only the Qualcomm binary driver exists for Adreno.
    vendor = DriverDetails::VENDOR_QUALCOMM;
    driver = DriverDetails::DRIVER_QUALCOMM;
  }
  else if (vendor_id == 0x13B6 || device_name.find("Mali") != std::string::npos)
  {
    // Currently only the ARM binary driver exists for Mali.
    vendor = DriverDetails::VENDOR_ARM;
    driver = DriverDetails::DRIVER_ARM;
  }
  else if (vendor_id == 0x1010 || device_name.find("PowerVR") != std::string::npos)
  {
    // Currently only the binary driver exists for PowerVR.
    vendor = DriverDetails::VENDOR_IMGTEC;
    driver = DriverDetails::DRIVER_IMGTEC;
  }
    else if (vendor_id == 0x14E4 || device_name.find("V3D 4.2") != std::string::npos)
  {
    // Supported by the videocore IV found in the RPI4 and upwards.
    vendor = DriverDetails::VENDOR_MESA;
    driver = DriverDetails::DRIVER_V3D;
  }
  else
  {
    WARN_LOG(VIDEO, "Unknown Vulkan driver vendor, please report it to us.");
    WARN_LOG(VIDEO, "Vendor ID: 0x%X, Device Name: %s", vendor_id, device_name.c_str());
    vendor = DriverDetails::VENDOR_UNKNOWN;
    driver = DriverDetails::DRIVER_UNKNOWN;
  }

#ifdef __APPLE__
  // Vulkan on macOS goes through Metal, and is not susceptible to the same bugs
  // as the vendor's native Vulkan drivers. We use a different driver fields to
  // differentiate MoltenVK.
  driver = DriverDetails::DRIVER_PORTABILITY;
#endif

  DriverDetails::Init(DriverDetails::API_VULKAN, vendor, driver,
                      static_cast<double>(m_device_properties.driverVersion),
                      DriverDetails::Family::UNKNOWN);
}

void VulkanContext::PopulateShaderSubgroupSupport()
{
  // Vulkan 1.1 support is required for vkGetPhysicalDeviceProperties2(), but we can't rely on the
  // function pointer alone.
  if (!vkGetPhysicalDeviceProperties2 || (VK_VERSION_MAJOR(m_device_properties.apiVersion) == 1 &&
                                          VK_VERSION_MINOR(m_device_properties.apiVersion) < 1))
  {
    return;
  }

  VkPhysicalDeviceProperties2 device_properties_2 = {};
  device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

  VkPhysicalDeviceSubgroupProperties subgroup_properties = {};
  subgroup_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
  device_properties_2.pNext = &subgroup_properties;

  vkGetPhysicalDeviceProperties2(m_physical_device, &device_properties_2);

  m_shader_subgroup_size = subgroup_properties.subgroupSize;

  // We require basic ops (for gl_SubgroupInvocationID), ballot (for subgroupBallot,
  // subgroupBallotFindLSB), and arithmetic (for subgroupMin/subgroupMax).
  constexpr VkSubgroupFeatureFlags required_operations = VK_SUBGROUP_FEATURE_BASIC_BIT |
                                                         VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
                                                         VK_SUBGROUP_FEATURE_BALLOT_BIT;
  m_supports_shader_subgroup_operations =
      (subgroup_properties.supportedOperations & required_operations) == required_operations &&
      subgroup_properties.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT;
}

bool VulkanContext::SupportsExclusiveFullscreen(const WindowSystemInfo& wsi, VkSurfaceKHR surface)
{
#ifdef SUPPORTS_VULKAN_EXCLUSIVE_FULLSCREEN
  if (!surface || !vkGetPhysicalDeviceSurfaceCapabilities2KHR ||
      !SupportsDeviceExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
  {
    return false;
  }

  VkPhysicalDeviceSurfaceInfo2KHR si = {};
  si.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
  si.surface = surface;

  auto platform_info = GetPlatformExclusiveFullscreenInfo(wsi);
  si.pNext = &platform_info;

  VkSurfaceCapabilities2KHR caps = {};
  caps.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

  VkSurfaceCapabilitiesFullScreenExclusiveEXT fullscreen_caps = {};
  fullscreen_caps.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT;
  fullscreen_caps.fullScreenExclusiveSupported = VK_TRUE;
  caps.pNext = &fullscreen_caps;

  VkResult res = vkGetPhysicalDeviceSurfaceCapabilities2KHR(m_physical_device, &si, &caps);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceCapabilities2KHR failed:");
    return false;
  }

  return fullscreen_caps.fullScreenExclusiveSupported;
#else
  return false;
#endif
}

#ifdef WIN32
VkSurfaceFullScreenExclusiveWin32InfoEXT
VulkanContext::GetPlatformExclusiveFullscreenInfo(const WindowSystemInfo& wsi)
{
  VkSurfaceFullScreenExclusiveWin32InfoEXT info = {};
  info.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
  info.hmonitor =
      MonitorFromWindow(static_cast<HWND>(wsi.render_surface), MONITOR_DEFAULTTOPRIMARY);
  return info;
}
#endif

}  // namespace Vulkan
