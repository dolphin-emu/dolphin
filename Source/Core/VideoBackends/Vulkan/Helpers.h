// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "VideoCommon/VideoConfig.h"

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
VkInstance CreateVulkanInstance(bool enable_debug_layer);

std::vector<const char*> SelectVulkanInstanceExtensions(bool enable_debug_layer);

bool CheckDebugLayerAvailability();

std::vector<VkPhysicalDevice> EnumerateVulkanPhysicalDevices(VkInstance instance);

std::vector<const char*> SelectVulkanDeviceExtensions(VkPhysicalDevice physical_device);

bool SelectVulkanDeviceFeatures(VkPhysicalDevice device, VkPhysicalDeviceFeatures* enable_features);

void PopulateBackendInfo(VideoConfig* config);

void PopulateBackendInfoAdapters(VideoConfig* config,
                                 const std::vector<VkPhysicalDevice>& physical_device_list);

void PopulateBackendInfoFeatures(VideoConfig* config, VkPhysicalDevice physical_device,
                                 const VkPhysicalDeviceFeatures& features);

void PopulateBackendInfoMultisampleModes(VideoConfig* config, VkPhysicalDevice physical_device,
                                         const VkPhysicalDeviceProperties& properties);

VkSurfaceKHR CreateVulkanSurface(VkInstance instance, void* hwnd);

VkDevice CreateVulkanDevice(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                            uint32_t* out_graphics_queue_family_index, VkQueue* out_graphics_queue,
                            uint32_t* out_present_queue_family_index, VkQueue* out_present_queue,
                            VkQueueFamilyProperties* out_graphics_queue_family_properties,
                            const VkPhysicalDeviceFeatures& features, bool enable_debug_layer);

VkPresentModeKHR SelectVulkanPresentMode(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

VkSurfaceFormatKHR SelectVulkanSurfaceFormat(VkPhysicalDevice physical_device,
                                             VkSurfaceKHR surface);

bool IsDepthFormat(VkFormat format);
VkFormat GetLinearFormat(VkFormat format);
u32 GetTexelSize(VkFormat format);

// Helper methods for cleaning up device objects, used by deferred destruction
struct DeferredResourceDestruction
{
  union Object {
    VkCommandPool CommandPool;
    VkDeviceMemory DeviceMemory;
    VkBuffer Buffer;
    VkBufferView BufferView;
    VkImage Image;
    VkImageView ImageView;
    VkRenderPass RenderPass;
    VkFramebuffer Framebuffer;
    VkSwapchainKHR Swapchain;
    VkShaderModule ShaderModule;
    VkPipeline Pipeline;
  } object;

  void (*destroy_callback)(VkDevice device, const Object& object);

  template <typename T>
  static DeferredResourceDestruction Wrapper(T object);
};

}  // namespace Vulkan
