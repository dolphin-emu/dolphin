#pragma once

#include <vector>

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

std::vector<const char*> SelectVulkanInstanceExtensions();
VkInstance CreateVulkanInstance();
std::vector<VkPhysicalDevice> EnumerateVulkanPhysicalDevices(VkInstance instance);
VkPhysicalDevice SelectVulkanPhysicalDevice(VkInstance instance, int device_index);
std::vector<const char*> SelectVulkanDeviceExtensions(VkPhysicalDevice physical_device);
VkSurfaceKHR CreateVulkanSurface(VkInstance instance, void* hwnd);
VkDevice CreateVulkanDevice(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
	uint32_t* out_graphics_queue_family_index, VkQueue* out_graphics_queue,
	uint32_t* out_present_queue_family_index, VkQueue* out_present_queue);

VkPresentModeKHR SelectVulkanPresentMode(VkPhysicalDevice physical_device, VkSurfaceKHR surface);
VkSurfaceFormatKHR SelectVulkanSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

bool IsDepthFormat(VkFormat format);

}		// namespace Vulkan
