// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
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
  static VkInstance CreateVulkanInstance(bool enable_surface, bool enable_debug_report,
                                         bool enable_validation_layer);

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
                                               VkSurfaceKHR surface, bool enable_debug_reports,
                                               bool enable_validation_layer);

  // Enable/disable debug message runtime.
  bool EnableDebugReports();
  void DisableDebugReports();

  // Global state accessors
  VkInstance GetVulkanInstance() const { return m_instance; }
  VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }
  VkDevice GetDevice() const { return m_device; }
  VkQueue GetGraphicsQueue() const { return m_graphics_queue; }
  u32 GetGraphicsQueueFamilyIndex() const { return m_graphics_queue_family_index; }
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
  bool SupportsGeometryShaders() const { return m_device_features.geometryShader == VK_TRUE; }
  bool SupportsDualSourceBlend() const { return m_device_features.dualSrcBlend == VK_TRUE; }
  bool SupportsLogicOps() const { return m_device_features.logicOp == VK_TRUE; }
  bool SupportsBoundingBox() const { return m_device_features.fragmentStoresAndAtomics == VK_TRUE; }
  bool SupportsPreciseOcclusionQueries() const
  {
    return m_device_features.occlusionQueryPrecise == VK_TRUE;
  }
  bool SupportsNVGLSLExtension() const { return m_supports_nv_glsl_extension; }
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
  // Finds a memory type index for the specified memory properties and the bits returned by
  // vkGetImageMemoryRequirements
  bool GetMemoryType(u32 bits, VkMemoryPropertyFlags properties, u32* out_type_index);
  u32 GetMemoryType(u32 bits, VkMemoryPropertyFlags properties);

  // Finds a memory type for upload or readback buffers.
  u32 GetUploadMemoryType(u32 bits, bool* is_coherent = nullptr);
  u32 GetReadbackMemoryType(u32 bits, bool* is_coherent = nullptr, bool* is_cached = nullptr);

private:
  using ExtensionList = std::vector<const char*>;
  static bool SelectInstanceExtensions(ExtensionList* extension_list, bool enable_surface,
                                       bool enable_debug_report);
  bool SelectDeviceExtensions(ExtensionList* extension_list, bool enable_surface);
  bool SelectDeviceFeatures();
  bool CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer);

  VkInstance m_instance = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;

  VkQueue m_graphics_queue = VK_NULL_HANDLE;
  u32 m_graphics_queue_family_index = 0;
  VkQueueFamilyProperties m_graphics_queue_properties = {};

  VkDebugReportCallbackEXT m_debug_report_callback = VK_NULL_HANDLE;

  VkPhysicalDeviceFeatures m_device_features = {};
  VkPhysicalDeviceProperties m_device_properties = {};
  VkPhysicalDeviceMemoryProperties m_device_memory_properties = {};

  bool m_supports_nv_glsl_extension = false;
};

extern std::unique_ptr<VulkanContext> g_vulkan_context;

}  // namespace Vulkan
