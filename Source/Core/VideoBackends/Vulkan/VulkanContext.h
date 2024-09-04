// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
class VulkanContext
{
public:
  struct PhysicalDeviceInfo
  {
    PhysicalDeviceInfo(const PhysicalDeviceInfo&) = default;
    explicit PhysicalDeviceInfo(VkPhysicalDevice device);
    VkPhysicalDeviceFeatures features() const;

    char deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    u8 pipelineCacheUUID[VK_UUID_SIZE];
    u32 apiVersion;
    u32 driverVersion;
    u32 vendorID;
    u32 deviceID;
    VkDeviceSize minUniformBufferOffsetAlignment;
    VkDeviceSize bufferImageGranularity;
    u32 maxTexelBufferElements;
    u32 maxImageDimension2D;
    VkSampleCountFlags framebufferColorSampleCounts;
    VkSampleCountFlags framebufferDepthSampleCounts;
    float pointSizeRange[2];
    float maxSamplerAnisotropy;
    u32 subgroupSize = 1;
    VkDriverId driverID = static_cast<VkDriverId>(0);
    bool dualSrcBlend;
    bool geometryShader;
    bool samplerAnisotropy;
    bool logicOp;
    bool fragmentStoresAndAtomics;
    bool sampleRateShading;
    bool largePoints;
    bool shaderStorageImageMultisample;
    bool shaderTessellationAndGeometryPointSize;
    bool occlusionQueryPrecise;
    bool shaderClipDistance;
    bool depthClamp;
    bool textureCompressionBC;
    bool shaderSubgroupOperations = false;
  };

  VulkanContext(VkInstance instance, VkPhysicalDevice physical_device);
  ~VulkanContext();

  // Determines if the Vulkan validation layer is available on the system.
  static bool CheckValidationLayerAvailablility();

  // Helper method to create a Vulkan instance.
  static VkInstance CreateVulkanInstance(WindowSystemType wstype, bool enable_debug_utils,
                                         bool enable_validation_layer, u32* out_vk_api_version);

  // Returns a list of Vulkan-compatible GPUs.
  using GPUList = std::vector<VkPhysicalDevice>;
  static GPUList EnumerateGPUs(VkInstance instance);

  // Populates backend/video config.
  // These are public so that the backend info can be populated without creating a context.
  static void PopulateBackendInfo(VideoConfig* config);
  static void PopulateBackendInfoAdapters(VideoConfig* config, const GPUList& gpu_list);
  static void PopulateBackendInfoFeatures(VideoConfig* config, VkPhysicalDevice gpu,
                                          const PhysicalDeviceInfo& info);
  static void PopulateBackendInfoMultisampleModes(VideoConfig* config, VkPhysicalDevice gpu,
                                                  const PhysicalDeviceInfo& info);

  // Creates a Vulkan device context.
  // This assumes that PopulateBackendInfo and PopulateBackendInfoAdapters has already
  // been called for the specified VideoConfig.
  static std::unique_ptr<VulkanContext> Create(VkInstance instance, VkPhysicalDevice gpu,
                                               VkSurfaceKHR surface, bool enable_debug_utils,
                                               bool enable_validation_layer, u32 api_version);

  // Enable/disable debug message runtime.
  bool EnableDebugUtils();
  void DisableDebugUtils();

  // Global state accessors
  VkInstance GetVulkanInstance() const { return m_instance; }
  VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }
  VkDevice GetDevice() const { return m_device; }
  VkQueue GetGraphicsQueue() const { return m_graphics_queue; }
  u32 GetGraphicsQueueFamilyIndex() const { return m_graphics_queue_family_index; }
  VkQueue GetPresentQueue() const { return m_present_queue; }
  u32 GetPresentQueueFamilyIndex() const { return m_present_queue_family_index; }
  const VkQueueFamilyProperties& GetGraphicsQueueProperties() const
  {
    return m_graphics_queue_properties;
  }
  const PhysicalDeviceInfo& GetDeviceInfo() const { return m_device_info; }
  // Support bits
  bool SupportsAnisotropicFiltering() const { return m_device_info.samplerAnisotropy; }
  bool SupportsPreciseOcclusionQueries() const { return m_device_info.occlusionQueryPrecise; }
  u32 GetShaderSubgroupSize() const { return m_device_info.subgroupSize; }
  bool SupportsShaderSubgroupOperations() const { return m_device_info.shaderSubgroupOperations; }

  // Helpers for getting constants
  VkDeviceSize GetUniformBufferAlignment() const
  {
    return m_device_info.minUniformBufferOffsetAlignment;
  }
  VkDeviceSize GetBufferImageGranularity() const { return m_device_info.bufferImageGranularity; }
  float GetMaxSamplerAnisotropy() const { return m_device_info.maxSamplerAnisotropy; }

  // Returns true if the specified extension is supported and enabled.
  bool SupportsDeviceExtension(const char* name) const;

  // Returns true if exclusive fullscreen is supported for the given surface.
  bool SupportsExclusiveFullscreen(const WindowSystemInfo& wsi, VkSurfaceKHR surface);

  VmaAllocator GetMemoryAllocator() const { return m_allocator; }

#ifdef WIN32
  // Returns the platform-specific exclusive fullscreen structure.
  VkSurfaceFullScreenExclusiveWin32InfoEXT
  GetPlatformExclusiveFullscreenInfo(const WindowSystemInfo& wsi);
#endif

private:
  static bool SelectInstanceExtensions(std::vector<const char*>* extension_list,
                                       WindowSystemType wstype, bool enable_debug_utils,
                                       bool validation_layer_enabled);
  bool SelectDeviceExtensions(bool enable_surface);
  void WarnMissingDeviceFeatures();
  bool CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer);
  void InitDriverDetails();
  bool CreateAllocator(u32 vk_api_version);

  VkInstance m_instance = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VmaAllocator m_allocator = VK_NULL_HANDLE;

  VkQueue m_graphics_queue = VK_NULL_HANDLE;
  u32 m_graphics_queue_family_index = 0;
  VkQueue m_present_queue = VK_NULL_HANDLE;
  u32 m_present_queue_family_index = 0;
  VkQueueFamilyProperties m_graphics_queue_properties = {};

  VkDebugUtilsMessengerEXT m_debug_utils_messenger = VK_NULL_HANDLE;

  PhysicalDeviceInfo m_device_info;

  std::vector<std::string> m_device_extensions;
};

extern std::unique_ptr<VulkanContext> g_vulkan_context;

}  // namespace Vulkan
