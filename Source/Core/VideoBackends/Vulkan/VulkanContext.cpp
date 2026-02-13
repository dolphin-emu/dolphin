// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VulkanContext.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include "Common/Assert.h"
#include "Common/Contains.h"
#include "Common/Logging/Log.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/VideoCommon.h"

namespace Vulkan
{
static constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

std::unique_ptr<VulkanContext> g_vulkan_context;

template <typename Chain, typename Element>
static void InsertIntoChain(Chain* chain, Element* element)
{
  element->pNext = chain->pNext;
  chain->pNext = element;
}

VulkanContext::PhysicalDeviceInfo::PhysicalDeviceInfo(VkPhysicalDevice device)
{
  VkPhysicalDeviceFeatures features;
  VkPhysicalDeviceProperties2 properties2;
  VkPhysicalDeviceProperties& properties = properties2.properties;

  vkGetPhysicalDeviceProperties(device, &properties);
  vkGetPhysicalDeviceFeatures(device, &features);
  apiVersion = vkGetPhysicalDeviceProperties2 ? properties.apiVersion : VK_API_VERSION_1_0;

  if (apiVersion >= VK_API_VERSION_1_1)
  {
    VkPhysicalDeviceSubgroupProperties properties_subgroup = {};
    VkPhysicalDeviceVulkan12Properties properties_vk12 = {};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = nullptr;
    properties_subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    InsertIntoChain(&properties2, &properties_subgroup);

    if (apiVersion >= VK_API_VERSION_1_2)
    {
      properties_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
      InsertIntoChain(&properties2, &properties_vk12);
    }

    vkGetPhysicalDeviceProperties2(device, &properties2);

    if (apiVersion >= VK_API_VERSION_1_2)
    {
      driverID = properties_vk12.driverID;
    }

    subgroupSize = properties_subgroup.subgroupSize;

    // We require basic ops (for gl_SubgroupInvocationID), ballot (for subgroupBallot,
    // subgroupBallotFindLSB), and arithmetic (for subgroupMin/subgroupMax).
    // Shuffle is enabled as a workaround until SPIR-V >= 1.5 is enabled with broadcast(uniform)
    // support.
    constexpr VkSubgroupFeatureFlags required_operations =
        VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
        VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT;
    shaderSubgroupOperations =
        (properties_subgroup.supportedOperations & required_operations) == required_operations &&
        properties_subgroup.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  memcpy(deviceName, properties.deviceName, sizeof(deviceName));
  memcpy(pipelineCacheUUID, properties.pipelineCacheUUID, sizeof(pipelineCacheUUID));
  vendorID = properties.vendorID;
  deviceID = properties.deviceID;
  minUniformBufferOffsetAlignment =
      std::max<VkDeviceSize>(properties.limits.minUniformBufferOffsetAlignment, 1);
  bufferImageGranularity = std::max<VkDeviceSize>(properties.limits.bufferImageGranularity, 1);
  maxTexelBufferElements = properties.limits.maxTexelBufferElements;
  maxImageDimension2D = properties.limits.maxImageDimension2D;
  framebufferColorSampleCounts = properties.limits.framebufferColorSampleCounts;
  framebufferDepthSampleCounts = properties.limits.framebufferDepthSampleCounts;
  memcpy(pointSizeRange, properties.limits.pointSizeRange, sizeof(pointSizeRange));
  maxSamplerAnisotropy = properties.limits.maxSamplerAnisotropy;

  dualSrcBlend = features.dualSrcBlend != VK_FALSE;
  geometryShader = features.geometryShader != VK_FALSE;
  samplerAnisotropy = features.samplerAnisotropy != VK_FALSE;
  logicOp = features.logicOp != VK_FALSE;
  fragmentStoresAndAtomics = features.fragmentStoresAndAtomics != VK_FALSE;
  sampleRateShading = features.sampleRateShading != VK_FALSE;
  largePoints = features.largePoints != VK_FALSE;
  shaderStorageImageMultisample = features.shaderStorageImageMultisample != VK_FALSE;
  shaderTessellationAndGeometryPointSize =
      features.shaderTessellationAndGeometryPointSize != VK_FALSE;
  occlusionQueryPrecise = features.occlusionQueryPrecise != VK_FALSE;
  shaderClipDistance = features.shaderClipDistance != VK_FALSE;
  depthClamp = features.depthClamp != VK_FALSE;
  textureCompressionBC = features.textureCompressionBC != VK_FALSE;
}

VkPhysicalDeviceFeatures VulkanContext::PhysicalDeviceInfo::features() const
{
  VkPhysicalDeviceFeatures features;
  memset(&features, 0, sizeof(features));
  features.dualSrcBlend = dualSrcBlend ? VK_TRUE : VK_FALSE;
  features.geometryShader = geometryShader ? VK_TRUE : VK_FALSE;
  features.samplerAnisotropy = samplerAnisotropy ? VK_TRUE : VK_FALSE;
  features.logicOp = logicOp ? VK_TRUE : VK_FALSE;
  features.fragmentStoresAndAtomics = fragmentStoresAndAtomics ? VK_TRUE : VK_FALSE;
  features.sampleRateShading = sampleRateShading ? VK_TRUE : VK_FALSE;
  features.largePoints = largePoints ? VK_TRUE : VK_FALSE;
  features.shaderStorageImageMultisample = shaderStorageImageMultisample ? VK_TRUE : VK_FALSE;
  features.shaderTessellationAndGeometryPointSize =
      shaderTessellationAndGeometryPointSize ? VK_TRUE : VK_FALSE;
  features.occlusionQueryPrecise = occlusionQueryPrecise ? VK_TRUE : VK_FALSE;
  features.shaderClipDistance = shaderClipDistance ? VK_TRUE : VK_FALSE;
  features.depthClamp = depthClamp ? VK_TRUE : VK_FALSE;
  features.textureCompressionBC = textureCompressionBC ? VK_TRUE : VK_FALSE;
  return features;
}

VulkanContext::VulkanContext(VkInstance instance, VkPhysicalDevice physical_device)
    : m_instance(instance), m_physical_device(physical_device), m_device_info(physical_device)
{
}

VulkanContext::~VulkanContext()
{
  if (m_allocator != VK_NULL_HANDLE)
    vmaDestroyAllocator(m_allocator);
  if (m_device != VK_NULL_HANDLE)
    vkDestroyDevice(m_device, nullptr);

  if (m_debug_utils_messenger != VK_NULL_HANDLE)
    DisableDebugUtils();

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

  bool supports_validation_layers = Common::Contains(
      layer_list, std::string_view{VALIDATION_LAYER_NAME}, &VkLayerProperties::layerName);

  bool supports_debug_utils =
      Common::Contains(extension_list, std::string_view{VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
                       &VkExtensionProperties::extensionName);

  if (!supports_debug_utils && supports_validation_layers)
  {
    // If the instance doesn't support debug utils but we're using validation layers,
    // try to use the implementation of the extension provided by the validation layers.
    extension_count = 0;
    res = vkEnumerateInstanceExtensionProperties(VALIDATION_LAYER_NAME, &extension_count, nullptr);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
      return false;
    }

    extension_list.resize(extension_count);
    res = vkEnumerateInstanceExtensionProperties(VALIDATION_LAYER_NAME, &extension_count,
                                                 extension_list.data());
    ASSERT(res == VK_SUCCESS);
    supports_debug_utils =
        Common::Contains(extension_list, std::string_view{VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
                         &VkExtensionProperties::extensionName);
  }

  // Check for both VK_EXT_debug_utils and VK_LAYER_KHRONOS_validation
  return supports_debug_utils && supports_validation_layers;
}

static u32 getAPIVersion()
{
  u32 supported_version;
  u32 used_version = VK_API_VERSION_1_0;
  if (vkEnumerateInstanceVersion && vkEnumerateInstanceVersion(&supported_version) == VK_SUCCESS)
  {
    // The device itself may not support 1.1, so we check that before using any 1.1 functionality.
    if (supported_version >= VK_API_VERSION_1_2)
      used_version = VK_API_VERSION_1_2;
    else if (supported_version >= VK_API_VERSION_1_1)
      used_version = VK_API_VERSION_1_1;
    WARN_LOG_FMT(HOST_GPU, "Using Vulkan 1.{}, supported: {}.{}", VK_VERSION_MINOR(used_version),
                 VK_VERSION_MAJOR(supported_version), VK_VERSION_MINOR(supported_version));
  }
  else
  {
    WARN_LOG_FMT(HOST_GPU, "Using Vulkan 1.0");
  }
  return used_version;
}

VkInstance VulkanContext::CreateVulkanInstance(WindowSystemType wstype, bool enable_debug_utils,
                                               bool enable_validation_layer,
                                               u32* out_vk_api_version)
{
  std::vector<const char*> enabled_extensions;
  if (!SelectInstanceExtensions(&enabled_extensions, wstype, enable_debug_utils,
                                enable_validation_layer))
    return VK_NULL_HANDLE;

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = nullptr;
  app_info.pApplicationName = "Dolphin Emulator";
  app_info.applicationVersion = VK_MAKE_VERSION(5, 0, 0);
  app_info.pEngineName = "Dolphin Emulator";
  app_info.engineVersion = VK_MAKE_VERSION(5, 0, 0);
  app_info.apiVersion = getAPIVersion();

  *out_vk_api_version = app_info.apiVersion;

  VkInstanceCreateInfo instance_create_info = {};
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pNext = nullptr;
  instance_create_info.flags = 0;
  instance_create_info.pApplicationInfo = &app_info;
  instance_create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
  instance_create_info.ppEnabledExtensionNames = enabled_extensions.data();
  instance_create_info.enabledLayerCount = 0;
  instance_create_info.ppEnabledLayerNames = nullptr;

  // Enable validation layer if the user enabled them in the settings
  if (enable_validation_layer)
  {
    instance_create_info.enabledLayerCount = 1;
    instance_create_info.ppEnabledLayerNames = &VALIDATION_LAYER_NAME;
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
                                             WindowSystemType wstype, bool enable_debug_utils,
                                             bool validation_layer_enabled)
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
    ERROR_LOG_FMT(VIDEO, "Vulkan: No extensions supported by instance.");
    return false;
  }

  std::vector<VkExtensionProperties> available_extension_list(extension_count);
  res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                               available_extension_list.data());
  ASSERT(res == VK_SUCCESS);

  u32 validation_layer_extension_count = 0;
  std::vector<VkExtensionProperties> validation_layer_extension_list;
  if (validation_layer_enabled)
  {
    res = vkEnumerateInstanceExtensionProperties(VALIDATION_LAYER_NAME,
                                                 &validation_layer_extension_count, nullptr);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res,
                       "vkEnumerateInstanceExtensionProperties failed for validation layers: ");
    }
    else
    {
      validation_layer_extension_list.resize(validation_layer_extension_count);
      res = vkEnumerateInstanceExtensionProperties(VALIDATION_LAYER_NAME,
                                                   &validation_layer_extension_count,
                                                   validation_layer_extension_list.data());
      ASSERT(res == VK_SUCCESS);
    }
  }

  for (const auto& extension_properties : available_extension_list)
    INFO_LOG_FMT(VIDEO, "Available extension: {}", extension_properties.extensionName);

  for (const auto& extension_properties : validation_layer_extension_list)
  {
    INFO_LOG_FMT(VIDEO, "Available extension in validation layer: {}",
                 extension_properties.extensionName);
  }

  auto AddExtension = [&](const char* name, bool required) {
    bool extension_supported =
        Common::Contains(available_extension_list, std::string_view{name},
                         &VkExtensionProperties::extensionName) ||
        Common::Contains(validation_layer_extension_list, std::string_view{name},
                         &VkExtensionProperties::extensionName);

    if (extension_supported)
    {
      INFO_LOG_FMT(VIDEO, "Enabling extension: {}", name);
      extension_list->push_back(name);
      return true;
    }

    if (required)
      ERROR_LOG_FMT(VIDEO, "Vulkan: Missing required extension {}.", name);

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

  AddExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, false);
  if (wstype != WindowSystemType::Headless)
  {
    AddExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, false);
  }

  // VK_EXT_debug_utils
  if (AddExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false))
  {
    g_backend_info.bSupportsSettingObjectNames = true;
  }
  else if (enable_debug_utils)
  {
    WARN_LOG_FMT(VIDEO, "Vulkan: Debug utils requested, but extension is not available.");
  }

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

void VulkanContext::PopulateBackendInfo(BackendInfo* backend_info)
{
  backend_info->api_type = APIType::Vulkan;
  backend_info->bSupports3DVision = false;                  // D3D-exclusive.
  backend_info->bSupportsEarlyZ = true;                     // Assumed support.
  backend_info->bSupportsPrimitiveRestart = true;           // Assumed support.
  backend_info->bSupportsBindingLayout = false;             // Assumed support.
  backend_info->bSupportsPaletteConversion = true;          // Assumed support.
  backend_info->bSupportsClipControl = true;                // Assumed support.
  backend_info->bSupportsMultithreading = true;             // Assumed support.
  backend_info->bSupportsComputeShaders = true;             // Assumed support.
  backend_info->bSupportsGPUTextureDecoding = true;         // Assumed support.
  backend_info->bSupportsBitfield = true;                   // Assumed support.
  backend_info->bSupportsPartialDepthCopies = true;         // Assumed support.
  backend_info->bSupportsShaderBinaries = true;             // Assumed support.
  backend_info->bSupportsPipelineCacheData = false;         // Handled via pipeline caches.
  backend_info->bSupportsDynamicSamplerIndexing = true;     // Assumed support.
  backend_info->bSupportsPostProcessing = true;             // Assumed support.
  backend_info->bSupportsBackgroundCompiling = true;        // Assumed support.
  backend_info->bSupportsCopyToVram = true;                 // Assumed support.
  backend_info->bSupportsReversedDepthRange = true;         // Assumed support.
  backend_info->bSupportsExclusiveFullscreen = false;       // Dependent on OS and features.
  backend_info->bSupportsDualSourceBlend = false;           // Dependent on features.
  backend_info->bSupportsGeometryShaders = false;           // Dependent on features.
  backend_info->bSupportsGSInstancing = false;              // Dependent on features.
  backend_info->bSupportsBBox = false;                      // Dependent on features.
  backend_info->bSupportsFragmentStoresAndAtomics = false;  // Dependent on features.
  backend_info->bSupportsSSAA = false;                      // Dependent on features.
  backend_info->bSupportsDepthClamp = false;                // Dependent on features.
  backend_info->bSupportsST3CTextures = false;              // Dependent on features.
  backend_info->bSupportsBPTCTextures = false;              // Dependent on features.
  backend_info->bSupportsLogicOp = false;                   // Dependent on features.
  backend_info->bSupportsLargePoints = false;               // Dependent on features.
  backend_info->bSupportsFramebufferFetch = false;          // Dependent on OS and features.
  backend_info->bSupportsCoarseDerivatives = true;          // Assumed support.
  backend_info->bSupportsTextureQueryLevels = true;         // Assumed support.
  backend_info->bSupportsLodBiasInSampler = false;          // Dependent on OS.
  backend_info->bSupportsSettingObjectNames = false;        // Dependent on features.
  backend_info->bSupportsPartialMultisampleResolve = true;  // Assumed support.
  backend_info->bSupportsDynamicVertexLoader = true;        // Assumed support.
  backend_info->bSupportsVSLinePointExpand = true;          // Assumed support.
  backend_info->bSupportsHDROutput = true;                  // Assumed support.
  backend_info->bSupportsUnrestrictedDepthRange = false;    // Dependent on features.
}

void VulkanContext::PopulateBackendInfoAdapters(BackendInfo* backend_info, const GPUList& gpu_list)
{
  backend_info->Adapters.clear();
  for (VkPhysicalDevice physical_device : gpu_list)
  {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    backend_info->Adapters.push_back(properties.deviceName);
  }
}

void VulkanContext::PopulateBackendInfoFeatures(BackendInfo* backend_info, VkPhysicalDevice gpu,
                                                const PhysicalDeviceInfo& info)
{
  backend_info->MaxTextureSize = info.maxImageDimension2D;
  backend_info->bUsesLowerLeftOrigin = false;
  backend_info->bSupportsDualSourceBlend = info.dualSrcBlend;
  backend_info->bSupportsGeometryShaders = info.geometryShader;
  backend_info->bSupportsGSInstancing = info.geometryShader;
  backend_info->bSupportsBBox = backend_info->bSupportsFragmentStoresAndAtomics =
      info.fragmentStoresAndAtomics;
  backend_info->bSupportsSSAA = info.sampleRateShading;
  backend_info->bSupportsLogicOp = info.logicOp;

  // Metal doesn't support this.
  backend_info->bSupportsLodBiasInSampler = info.driverID != VK_DRIVER_ID_MOLTENVK;

  // Disable geometry shader when shaderTessellationAndGeometryPointSize is not supported.
  // Seems this is needed for gl_Layer.
  if (!info.shaderTessellationAndGeometryPointSize)
  {
    backend_info->bSupportsGeometryShaders = VK_FALSE;
    backend_info->bSupportsGSInstancing = VK_FALSE;
  }

  // Depth clamping implies shaderClipDistance and depthClamp
  backend_info->bSupportsDepthClamp = info.depthClamp && info.shaderClipDistance;

  // textureCompressionBC implies BC1 through BC7, which is a superset of DXT1/3/5, which we need.
  backend_info->bSupportsST3CTextures = info.textureCompressionBC;
  backend_info->bSupportsBPTCTextures = info.textureCompressionBC;

  // Some devices don't support point sizes >1 (e.g. Adreno).
  // If we can't use a point size above our maximum IR, use triangles instead for EFB pokes.
  // This means a 6x increase in the size of the vertices, though.
  backend_info->bSupportsLargePoints =
      info.largePoints && info.pointSizeRange[0] <= 1.0f && info.pointSizeRange[1] >= 16;

  std::string device_name = info.deviceName;
  u32 vendor_id = info.vendorID;
  bool is_moltenvk = info.driverID == VK_DRIVER_ID_MOLTENVK;

  // Only Apple family GPUs support framebuffer fetch.
  // We currently use a hacked MoltenVK to implement this, so don't attempt outside of MVK
  if (is_moltenvk && (vendor_id == 0x106B || device_name.find("Apple") != std::string::npos))
  {
    backend_info->bSupportsFramebufferFetch = true;
  }

  // Our usage of primitive restart appears to be broken on AMD's binary drivers.
  // Seems to be fine on GCN Gen 1-2, unconfirmed on GCN Gen 3, causes driver resets on GCN Gen 4.
  if (DriverDetails::HasBug(DriverDetails::BUG_PRIMITIVE_RESTART))
    backend_info->bSupportsPrimitiveRestart = false;

  // Reversed depth range is broken on some drivers, or is broken when used in combination
  // with depth clamping. Fall back to inverted depth range for these.
  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_REVERSED_DEPTH_RANGE))
    backend_info->bSupportsReversedDepthRange = false;

  // Dynamic sampler indexing locks up Intel GPUs on MoltenVK/Metal
  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DYNAMIC_SAMPLER_INDEXING))
    backend_info->bSupportsDynamicSamplerIndexing = false;

  // Check for VK_EXT_shader_viewport_index_layer extension support
  // This allows gl_Layer output from vertex shaders for stereo rendering without geometry shaders
  u32 extension_count = 0;
  vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extension_count, nullptr);
  if (extension_count > 0)
  {
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extension_count, extensions.data());
    for (const auto& ext : extensions)
    {
      if (std::strcmp(ext.extensionName, VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME) == 0)
      {
        backend_info->bSupportsVSLayerOutput = true;
        break;
      }
    }
  }
}

void VulkanContext::PopulateBackendInfoMultisampleModes(BackendInfo* backend_info,
                                                        VkPhysicalDevice gpu,
                                                        const PhysicalDeviceInfo& info)
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
  VkSampleCountFlags supported_sample_counts =
      info.framebufferColorSampleCounts & info.framebufferDepthSampleCounts &
      efb_color_properties.sampleCounts & efb_depth_properties.sampleCounts;

  // No AA
  backend_info->AAModes.clear();
  backend_info->AAModes.emplace_back(1);

  // 2xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_2_BIT)
    backend_info->AAModes.emplace_back(2);

  // 4xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_4_BIT)
    backend_info->AAModes.emplace_back(4);

  // 8xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_8_BIT)
    backend_info->AAModes.emplace_back(8);

  // 16xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_16_BIT)
    backend_info->AAModes.emplace_back(16);

  // 32xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_32_BIT)
    backend_info->AAModes.emplace_back(32);

  // 64xMSAA/SSAA
  if (supported_sample_counts & VK_SAMPLE_COUNT_64_BIT)
    backend_info->AAModes.emplace_back(64);
}

std::unique_ptr<VulkanContext> VulkanContext::Create(VkInstance instance, VkPhysicalDevice gpu,
                                                     VkSurfaceKHR surface, bool enable_debug_utils,
                                                     bool enable_validation_layer,
                                                     u32 vk_api_version)
{
  std::unique_ptr<VulkanContext> context = std::make_unique<VulkanContext>(instance, gpu);

  // Initialize DriverDetails so that we can check for bugs to disable features if needed.
  context->InitDriverDetails();

  // Enable debug messages if the "Host GPU" log category is enabled.
  if (enable_debug_utils)
    context->EnableDebugUtils();

  // Attempt to create the device.
  if (!context->CreateDevice(surface, enable_validation_layer) ||
      !context->CreateAllocator(vk_api_version))
  {
    // Since we are destroying the instance, we're also responsible for destroying the surface.
    if (surface != VK_NULL_HANDLE)
      vkDestroySurfaceKHR(instance, surface, nullptr);

    return nullptr;
  }

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
    ERROR_LOG_FMT(VIDEO, "Vulkan: No extensions supported by device.");
    return false;
  }

  std::vector<VkExtensionProperties> available_extension_list(extension_count);
  res = vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &extension_count,
                                             available_extension_list.data());
  ASSERT(res == VK_SUCCESS);

  for (const auto& extension_properties : available_extension_list)
    INFO_LOG_FMT(VIDEO, "Available extension: {}", extension_properties.extensionName);

  auto AddExtension = [&](const char* name, bool required) {
    if (Common::Contains(available_extension_list, std::string_view{name},
                         &VkExtensionProperties::extensionName))
    {
      INFO_LOG_FMT(VIDEO, "Enabling extension: {}", name);
      m_device_extensions.push_back(name);
      return true;
    }

    if (required)
      ERROR_LOG_FMT(VIDEO, "Vulkan: Missing required extension {}.", name);

    return false;
  };

  if (enable_surface && !AddExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME, true))
    return false;

#ifdef SUPPORTS_VULKAN_EXCLUSIVE_FULLSCREEN
  // VK_EXT_full_screen_exclusive
  if (AddExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME, true))
    INFO_LOG_FMT(VIDEO, "Using VK_EXT_full_screen_exclusive for exclusive fullscreen.");
#endif

  AddExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, false);
  AddExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, false);

  // VK_EXT_shader_viewport_index_layer allows gl_Layer output from vertex shaders
  // This enables 2-layer stereo rendering without geometry shaders
  if (AddExtension(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME, false))
  {
    g_backend_info.bSupportsVSLayerOutput = true;
    INFO_LOG_FMT(VIDEO, "Using VK_EXT_shader_viewport_index_layer for vertex shader layer output.");
  }

  if (!DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DEPTH_CLAMP_CONTROL))
  {
    // Unrestricted depth range is one of the few extensions that changes the behavior
    // of Vulkan just by being enabled, so we rely on lazy evaluation to ensure it is
    // not enabled unless depth clamp control is supported.
    g_backend_info.bSupportsUnrestrictedDepthRange =
        AddExtension(VK_EXT_DEPTH_CLAMP_CONTROL_EXTENSION_NAME, false) &&
        AddExtension(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME, false);
  }

  return true;
}

void VulkanContext::WarnMissingDeviceFeatures()
{
  if (!m_device_info.largePoints)
    WARN_LOG_FMT(VIDEO, "Vulkan: Missing large points feature. CPU EFB writes will be slower.");
  if (!m_device_info.occlusionQueryPrecise)
  {
    WARN_LOG_FMT(VIDEO,
                 "Vulkan: Missing precise occlusion queries. Perf queries will be inaccurate.");
  }
}

bool VulkanContext::CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer)
{
  u32 queue_family_count;
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
  if (queue_family_count == 0)
  {
    ERROR_LOG_FMT(VIDEO, "No queue families found on specified vulkan physical device.");
    return false;
  }

  std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count,
                                           queue_family_properties.data());
  INFO_LOG_FMT(VIDEO, "{} vulkan queue families", queue_family_count);

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
    ERROR_LOG_FMT(VIDEO, "Vulkan: Failed to find an acceptable graphics queue.");
    return false;
  }
  if (surface && m_present_queue_family_index == queue_family_count)
  {
    ERROR_LOG_FMT(VIDEO, "Vulkan: Failed to find an acceptable present queue.");
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
  if (m_graphics_queue_family_index != m_present_queue_family_index &&
      m_present_queue_family_index != queue_family_count)
  {
    device_info.queueCreateInfoCount = 2;
  }
  device_info.pQueueCreateInfos = queue_infos.data();

  if (!SelectDeviceExtensions(surface != VK_NULL_HANDLE))
    return false;

  // convert std::string list to a char pointer list which we can feed in
  std::vector<const char*> extension_name_pointers;
  for (const std::string& name : m_device_extensions)
    extension_name_pointers.push_back(name.c_str());

  device_info.enabledLayerCount = 0;
  device_info.ppEnabledLayerNames = nullptr;
  device_info.enabledExtensionCount = static_cast<uint32_t>(extension_name_pointers.size());
  device_info.ppEnabledExtensionNames = extension_name_pointers.data();

  WarnMissingDeviceFeatures();

  VkPhysicalDeviceFeatures device_features = m_device_info.features();
  device_info.pEnabledFeatures = &device_features;

  // Enable debug layer on debug builds
  if (enable_validation_layer)
  {
    device_info.enabledLayerCount = 1;
    device_info.ppEnabledLayerNames = &VALIDATION_LAYER_NAME;
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

bool VulkanContext::CreateAllocator(u32 vk_api_version)
{
  VmaAllocatorCreateInfo allocator_info = {};
  allocator_info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
  allocator_info.physicalDevice = m_physical_device;
  allocator_info.device = m_device;
  allocator_info.preferredLargeHeapBlockSize = 64 << 20;
  allocator_info.pAllocationCallbacks = nullptr;
  allocator_info.pDeviceMemoryCallbacks = nullptr;
  allocator_info.pHeapSizeLimit = nullptr;
  allocator_info.pVulkanFunctions = nullptr;
  allocator_info.instance = m_instance;
  allocator_info.vulkanApiVersion = vk_api_version;
  allocator_info.pTypeExternalMemoryHandleTypes = nullptr;

  if (SupportsDeviceExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
    allocator_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

  VkResult res = vmaCreateAllocator(&allocator_info, &m_allocator);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vmaCreateAllocator failed: ");
    return false;
  }

  return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                   VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
  const std::string log_message = fmt::format("Vulkan debug message: {}", pCallbackData->pMessage);
  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    ERROR_LOG_FMT(HOST_GPU, "{}", log_message);
  else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    WARN_LOG_FMT(HOST_GPU, "{}", log_message);
  else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    INFO_LOG_FMT(HOST_GPU, "{}", log_message);
  else
    DEBUG_LOG_FMT(HOST_GPU, "{}", log_message);

  return VK_FALSE;
}

bool VulkanContext::EnableDebugUtils()
{
  // Already enabled?
  if (m_debug_utils_messenger != VK_NULL_HANDLE)
    return true;

  // Check for presence of the functions before calling
  if (!vkCreateDebugUtilsMessengerEXT || !vkDestroyDebugUtilsMessengerEXT ||
      !vkSubmitDebugUtilsMessageEXT)
  {
    return false;
  }

  VkDebugUtilsMessengerCreateInfoEXT messenger_info = {
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      nullptr,
      0,
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      DebugUtilsCallback,
      nullptr};

  VkResult res = vkCreateDebugUtilsMessengerEXT(m_instance, &messenger_info, nullptr,
                                                &m_debug_utils_messenger);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateDebugUtilsMessengerEXT failed: ");
    return false;
  }

  return true;
}

void VulkanContext::DisableDebugUtils()
{
  if (m_debug_utils_messenger != VK_NULL_HANDLE)
  {
    vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_utils_messenger, nullptr);
    m_debug_utils_messenger = VK_NULL_HANDLE;
  }
}

bool VulkanContext::SupportsDeviceExtension(const char* name) const
{
  return std::ranges::any_of(m_device_extensions,
                             [name](const std::string& extension) { return extension == name; });
}

static bool DriverIsMesa(VkDriverId driver_id)
{
  switch (driver_id)
  {
  case VK_DRIVER_ID_MESA_RADV:
  case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
  case VK_DRIVER_ID_MESA_LLVMPIPE:
  case VK_DRIVER_ID_MESA_TURNIP:
  case VK_DRIVER_ID_MESA_V3DV:
  case VK_DRIVER_ID_MESA_PANVK:
  case VK_DRIVER_ID_MESA_VENUS:
  case VK_DRIVER_ID_MESA_DOZEN:
  case VK_DRIVER_ID_MESA_NVK:
  case VK_DRIVER_ID_IMAGINATION_OPEN_SOURCE_MESA:
  case VK_DRIVER_ID_MESA_HONEYKRISP:
    return true;
  default:
    return false;
  }
}

static DriverDetails::Driver GetMesaDriver(VkDriverId driver_id)
{
  switch (driver_id)
  {
    // clang-format off
  case VK_DRIVER_ID_MESA_RADV:              return DriverDetails::DRIVER_R600;
  case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA: return DriverDetails::DRIVER_I965;
  case VK_DRIVER_ID_MESA_NVK:               return DriverDetails::DRIVER_NOUVEAU;
  case VK_DRIVER_ID_MESA_TURNIP:            return DriverDetails::DRIVER_FREEDRENO;
  default:                                  return DriverDetails::DRIVER_UNKNOWN;
    // clang-format on
  }
}

void VulkanContext::InitDriverDetails()
{
  DriverDetails::Vendor vendor;
  DriverDetails::Driver driver;

  // String comparisons aren't ideal, but there doesn't seem to be any other way to tell
  // which vendor a driver is for. These names are based on the reports submitted to
  // vulkan.gpuinfo.org, as of 19/09/2017.
  std::string device_name = m_device_info.deviceName;
  u32 vendor_id = m_device_info.vendorID;
  // Note: driver_id may be 0 on vulkan < 1.2
  VkDriverId driver_id = m_device_info.driverID;

  if (DriverIsMesa(driver_id))
  {
    vendor = DriverDetails::VENDOR_MESA;
    driver = GetMesaDriver(driver_id);
  }
  else if (vendor_id == 0x10DE)
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
// using anv if we're not running on Windows or macOS.
#if defined(WIN32) || defined(__APPLE__)
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
  else if (device_name.find("Apple") != std::string::npos)
  {
    vendor = DriverDetails::VENDOR_APPLE;
    driver = DriverDetails::DRIVER_PORTABILITY;
  }
  else
  {
    WARN_LOG_FMT(VIDEO, "Unknown Vulkan driver vendor, please report it to us.");
    WARN_LOG_FMT(VIDEO, "Vendor ID: {:#X}, Device Name: {}", vendor_id, device_name);
    vendor = DriverDetails::VENDOR_UNKNOWN;
    driver = DriverDetails::DRIVER_UNKNOWN;
  }

  // Vulkan on macOS goes through Metal, and is not susceptible to the same bugs
  // as the vendor's native Vulkan drivers. We use a different driver fields to
  // differentiate MoltenVK.
  if (driver_id == VK_DRIVER_ID_MOLTENVK)
    driver = DriverDetails::DRIVER_PORTABILITY;

  DriverDetails::Init(DriverDetails::API_VULKAN, vendor, driver,
                      static_cast<double>(m_device_info.driverVersion),
                      DriverDetails::Family::UNKNOWN, std::move(device_name));
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
