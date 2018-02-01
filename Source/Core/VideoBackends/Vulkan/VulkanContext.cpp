// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/DriverDetails.h"

namespace Vulkan
{
std::unique_ptr<VulkanContext> g_vulkan_context;

VulkanContext::VulkanContext(VkInstance instance, VkPhysicalDevice physical_device)
    : m_instance(instance), m_physical_device(physical_device)
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
  _assert_(res == VK_SUCCESS);

  u32 layer_count = 0;
  res = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
    return false;
  }

  std::vector<VkLayerProperties> layer_list(layer_count);
  res = vkEnumerateInstanceLayerProperties(&layer_count, layer_list.data());
  _assert_(res == VK_SUCCESS);

  // Check for both VK_EXT_debug_report and VK_LAYER_LUNARG_standard_validation
  return (std::find_if(extension_list.begin(), extension_list.end(),
                       [](const VkExtensionProperties& it) {
                         return strcmp(it.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0;
                       }) != extension_list.end() &&
          std::find_if(layer_list.begin(), layer_list.end(), [](const VkLayerProperties& it) {
            return strcmp(it.layerName, "VK_LAYER_LUNARG_standard_validation") == 0;
          }) != layer_list.end());
}

VkInstance VulkanContext::CreateVulkanInstance(bool enable_surface, bool enable_debug_report,
                                               bool enable_validation_layer)
{
  ExtensionList enabled_extensions;
  if (!SelectInstanceExtensions(&enabled_extensions, enable_surface, enable_debug_report))
    return VK_NULL_HANDLE;

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

bool VulkanContext::SelectInstanceExtensions(ExtensionList* extension_list, bool enable_surface,
                                             bool enable_debug_report)
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
  _assert_(res == VK_SUCCESS);

#if !(defined(_MSC_VER) && _MSC_VER <= 1800)
  for (const auto& extension_properties : available_extension_list)
    INFO_LOG(VIDEO, "Available extension: %s", extension_properties.extensionName);
#endif

  auto SupportsExtension = [&](const char* name, bool required) {
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
  if (enable_surface && !SupportsExtension(VK_KHR_SURFACE_EXTENSION_NAME, true))
    return false;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
  if (enable_surface && !SupportsExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true))
    return false;
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
  if (enable_surface && !SupportsExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME, true))
    return false;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
  if (enable_surface && !SupportsExtension(VK_KHR_XCB_SURFACE_EXTENSION_NAME, true))
    return false;
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
  if (enable_surface && !SupportsExtension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, true))
    return false;
#endif

  // VK_EXT_debug_report
  if (enable_debug_report && !SupportsExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, false))
    WARN_LOG(VIDEO, "Vulkan: Debug report requested, but extension is not available.");

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
  config->backend_info.bSupportsExclusiveFullscreen = false;  // Currently WSI does not allow this.
  config->backend_info.bSupports3DVision = false;             // D3D-exclusive.
  config->backend_info.bSupportsOversizedViewports = true;    // Assumed support.
  config->backend_info.bSupportsEarlyZ = true;                // Assumed support.
  config->backend_info.bSupportsPrimitiveRestart = true;      // Assumed support.
  config->backend_info.bSupportsBindingLayout = false;        // Assumed support.
  config->backend_info.bSupportsPaletteConversion = true;     // Assumed support.
  config->backend_info.bSupportsClipControl = true;           // Assumed support.
  config->backend_info.bSupportsMultithreading = true;        // Assumed support.
  config->backend_info.bSupportsComputeShaders = true;        // Assumed support.
  config->backend_info.bSupportsGPUTextureDecoding = true;    // Assumed support.
  config->backend_info.bSupportsBitfield = true;              // Assumed support.
  config->backend_info.bSupportsDynamicSamplerIndexing = true;        // Assumed support.
  config->backend_info.bSupportsInternalResolutionFrameDumps = true;  // Assumed support.
  config->backend_info.bSupportsPostProcessing = true;                // Assumed support.
  config->backend_info.bSupportsDualSourceBlend = false;              // Dependent on features.
  config->backend_info.bSupportsGeometryShaders = false;              // Dependent on features.
  config->backend_info.bSupportsGSInstancing = false;                 // Dependent on features.
  config->backend_info.bSupportsBBox = false;                         // Dependent on features.
  config->backend_info.bSupportsFragmentStoresAndAtomics = false;     // Dependent on features.
  config->backend_info.bSupportsSSAA = false;                         // Dependent on features.
  config->backend_info.bSupportsDepthClamp = false;                   // Dependent on features.
  config->backend_info.bSupportsST3CTextures = false;                 // Dependent on features.
  config->backend_info.bSupportsBPTCTextures = false;                 // Dependent on features.
  config->backend_info.bSupportsReversedDepthRange = false;  // No support yet due to driver bugs.
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
  config->backend_info.bSupportsDualSourceBlend = (features.dualSrcBlend == VK_TRUE);
  config->backend_info.bSupportsGeometryShaders = (features.geometryShader == VK_TRUE);
  config->backend_info.bSupportsGSInstancing = (features.geometryShader == VK_TRUE);
  config->backend_info.bSupportsBBox = config->backend_info.bSupportsFragmentStoresAndAtomics =
      (features.fragmentStoresAndAtomics == VK_TRUE);
  config->backend_info.bSupportsSSAA = (features.sampleRateShading == VK_TRUE);

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

  // Our usage of primitive restart appears to be broken on AMD's binary drivers.
  // Seems to be fine on GCN Gen 1-2, unconfirmed on GCN Gen 3, causes driver resets on GCN Gen 4.
  if (DriverDetails::HasBug(DriverDetails::BUG_PRIMITIVE_RESTART))
    config->backend_info.bSupportsPrimitiveRestart = false;
}

void VulkanContext::PopulateBackendInfoMultisampleModes(
    VideoConfig* config, VkPhysicalDevice gpu, const VkPhysicalDeviceProperties& properties)
{
  // Query image support for the EFB texture formats.
  VkImageFormatProperties efb_color_properties = {};
  vkGetPhysicalDeviceImageFormatProperties(
      gpu, EFB_COLOR_TEXTURE_FORMAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &efb_color_properties);
  VkImageFormatProperties efb_depth_properties = {};
  vkGetPhysicalDeviceImageFormatProperties(
      gpu, EFB_DEPTH_TEXTURE_FORMAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
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
  std::unique_ptr<VulkanContext> context = std::make_unique<VulkanContext>(instance, gpu);

  // Initialize DriverDetails so that we can check for bugs to disable features if needed.
  DriverDetails::Init(DriverDetails::API_VULKAN,
                      DriverDetails::TranslatePCIVendorID(context->m_device_properties.vendorID),
                      DriverDetails::DRIVER_UNKNOWN,
                      static_cast<double>(context->m_device_properties.driverVersion),
                      DriverDetails::Family::UNKNOWN);

  // Enable debug reports if the "Host GPU" log category is enabled.
  if (enable_debug_reports)
    context->EnableDebugReports();

  // Attempt to create the device.
  if (!context->CreateDevice(surface, enable_validation_layer))
  {
    // Since we are destroying the instance, we're also responsible for destroying the surface.
    if (surface != VK_NULL_HANDLE)
      vkDestroySurfaceKHR(instance, surface, nullptr);

    return nullptr;
  }

  return context;
}

bool VulkanContext::SelectDeviceExtensions(ExtensionList* extension_list, bool enable_surface)
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
  _assert_(res == VK_SUCCESS);

#if !(defined(_MSC_VER) && _MSC_VER <= 1800)
  for (const auto& extension_properties : available_extension_list)
    INFO_LOG(VIDEO, "Available extension: %s", extension_properties.extensionName);
#endif

  auto SupportsExtension = [&](const char* name, bool required) {
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

  if (enable_surface && !SupportsExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME, true))
    return false;

  m_supports_nv_glsl_extension = SupportsExtension(VK_NV_GLSL_SHADER_EXTENSION_NAME, false);
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

  // Check push constant size.
  if (properties.limits.maxPushConstantsSize < static_cast<u32>(PUSH_CONSTANT_BUFFER_SIZE))
  {
    PanicAlert("Vulkan: Push contant buffer size %u is below minimum %u.",
               properties.limits.maxPushConstantsSize, static_cast<u32>(PUSH_CONSTANT_BUFFER_SIZE));

    return false;
  }

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

bool VulkanContext::CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer)
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
      if (!surface)
      {
        break;
      }
    }

    if (surface)
    {
      VkBool32 present_supported;
      VkResult res =
          vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, surface, &present_supported);
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
  if (surface && m_present_queue_family_index == queue_family_count)
  {
    ERROR_LOG(VIDEO, "Vulkan: Failed to find an acceptable present queue.");
    return false;
  }

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = nullptr;
  device_info.flags = 0;

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr const
#endif
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
      graphics_queue_info, present_queue_info,
  }};

  device_info.queueCreateInfoCount = 1;
  if (m_graphics_queue_family_index != m_present_queue_family_index)
  {
    device_info.queueCreateInfoCount = 2;
  }
  device_info.pQueueCreateInfos = queue_infos.data();

  ExtensionList enabled_extensions;
  if (!SelectDeviceExtensions(&enabled_extensions, surface != VK_NULL_HANDLE))
    return false;

  device_info.enabledLayerCount = 0;
  device_info.ppEnabledLayerNames = nullptr;
  device_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
  device_info.ppEnabledExtensionNames = enabled_extensions.data();

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
  if (surface)
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
    GENERIC_LOG(LogTypes::HOST_GPU, LogTypes::LERROR, "%s", log_message.c_str())
  else if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
    GENERIC_LOG(LogTypes::HOST_GPU, LogTypes::LWARNING, "%s", log_message.c_str())
  else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    GENERIC_LOG(LogTypes::HOST_GPU, LogTypes::LINFO, "%s", log_message.c_str())
  else
    GENERIC_LOG(LogTypes::HOST_GPU, LogTypes::LDEBUG, "%s", log_message.c_str())

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

bool VulkanContext::GetMemoryType(u32 bits, VkMemoryPropertyFlags properties, u32* out_type_index)
{
  for (u32 i = 0; i < VK_MAX_MEMORY_TYPES; i++)
  {
    if ((bits & (1 << i)) != 0)
    {
      u32 supported = m_device_memory_properties.memoryTypes[i].propertyFlags & properties;
      if (supported == properties)
      {
        *out_type_index = i;
        return true;
      }
    }
  }

  return false;
}

u32 VulkanContext::GetMemoryType(u32 bits, VkMemoryPropertyFlags properties)
{
  u32 type_index = VK_MAX_MEMORY_TYPES;
  if (!GetMemoryType(bits, properties, &type_index))
    PanicAlert("Unable to find memory type for %x:%x", bits, properties);

  return type_index;
}

u32 VulkanContext::GetUploadMemoryType(u32 bits, bool* is_coherent)
{
  // Try for coherent memory first.
  VkMemoryPropertyFlags flags =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  u32 type_index;
  if (!GetMemoryType(bits, flags, &type_index))
  {
    WARN_LOG(
        VIDEO,
        "Vulkan: Failed to find a coherent memory type for uploads, this will affect performance.");

    // Try non-coherent memory.
    flags &= ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!GetMemoryType(bits, flags, &type_index))
    {
      // We shouldn't have any memory types that aren't host-visible.
      PanicAlert("Unable to get memory type for upload.");
      type_index = 0;
    }
  }

  if (is_coherent)
    *is_coherent = ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0);

  return type_index;
}

u32 VulkanContext::GetReadbackMemoryType(u32 bits, bool* is_coherent, bool* is_cached)
{
  // Try for cached and coherent memory first.
  VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  u32 type_index;
  if (!GetMemoryType(bits, flags, &type_index))
  {
    // For readbacks, caching is more important than coherency.
    flags &= ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!GetMemoryType(bits, flags, &type_index))
    {
      WARN_LOG(VIDEO, "Vulkan: Failed to find a cached memory type for readbacks, this will affect "
                      "performance.");

      // Remove the cached bit as well.
      flags &= ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
      if (!GetMemoryType(bits, flags, &type_index))
      {
        // We shouldn't have any memory types that aren't host-visible.
        PanicAlert("Unable to get memory type for upload.");
        type_index = 0;
      }
    }
  }

  if (is_coherent)
    *is_coherent = ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0);
  if (is_cached)
    *is_cached = ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0);

  return type_index;
}
}
