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
                                          const VkPhysicalDeviceProperties& properties,
                                          const VkPhysicalDeviceFeatures& features);
  static void PopulateBackendInfoMultisampleModes(VideoConfig* config, VkPhysicalDevice gpu,
                                                  const VkPhysicalDeviceProperties& properties);

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
  const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const
  {
    return m_device_memory_properties;
  }
  const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_device_properties; }
  const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_device_features; }
  const VkPhysicalDeviceLimits& GetDeviceLimits() const { return m_device_properties.limits; }
  // Support bits
  bool SupportsAnisotropicFiltering() const
  {
    return m_device_features.samplerAnisotropy == VK_TRUE;
  }
  bool SupportsPreciseOcclusionQueries() const
  {
    return m_device_features.occlusionQueryPrecise == VK_TRUE;
  }
  u32 GetShaderSubgroupSize() const { return m_shader_subgroup_size; }
  bool SupportsShaderSubgroupOperations() const { return m_supports_shader_subgroup_operations; }

  // Helpers for getting constants
  VkDeviceSize GetUniformBufferAlignment() const
  {
    return m_device_properties.limits.minUniformBufferOffsetAlignment;
  }
  VkDeviceSize GetTexelBufferAlignment() const
  {
    return m_device_properties.limits.minUniformBufferOffsetAlignment;
  }
  VkDeviceSize GetBufferImageGranularity() const
  {
    return m_device_properties.limits.bufferImageGranularity;
  }
  float GetMaxSamplerAnisotropy() const { return m_device_properties.limits.maxSamplerAnisotropy; }

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
  bool SelectDeviceFeatures();
  bool CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer);
  void InitDriverDetails();
  void PopulateShaderSubgroupSupport();
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

  VkPhysicalDeviceFeatures m_device_features = {};
  VkPhysicalDeviceProperties m_device_properties = {};
  VkPhysicalDeviceMemoryProperties m_device_memory_properties = {};

  u32 m_shader_subgroup_size = 1;
  bool m_supports_shader_subgroup_operations = false;

  std::vector<std::string> m_device_extensions;
};

extern std::unique_ptr<VulkanContext> g_vulkan_context;

}  // namespace Vulkan
