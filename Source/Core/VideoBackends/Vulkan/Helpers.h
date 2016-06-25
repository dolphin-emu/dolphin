// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "VideoBackends/Vulkan/Globals.h"
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

// Helper methods for cleaning up device objects, used by deferred destruction
struct DeferredResourceDestruction
{
	union Object
	{
		VkCommandPool CommandPool;
		VkDeviceMemory DeviceMemory;
		VkBuffer Buffer;
		VkBufferView BufferView;
		VkImage Image;
		VkImageView ImageView;
	} object;

	void(*destroy_callback)(VkDevice device, const Object& object);

	template<typename T> static DeferredResourceDestruction Wrapper(T object);

	template<> static DeferredResourceDestruction Wrapper<VkCommandPool>(VkCommandPool object)
	{
		DeferredResourceDestruction ret;
		ret.object.CommandPool = object;
		ret.destroy_callback = [](VkDevice device, const Object& obj) { vkDestroyCommandPool(device, obj.CommandPool, nullptr); };
		return ret;
	}

	template<> static DeferredResourceDestruction Wrapper<VkDeviceMemory>(VkDeviceMemory object)
	{
		DeferredResourceDestruction ret;
		ret.object.DeviceMemory = object;
		ret.destroy_callback = [](VkDevice device, const Object& obj) { vkFreeMemory(device, obj.DeviceMemory, nullptr); };
		return ret;
	}

	template<> static DeferredResourceDestruction Wrapper<VkBuffer>(VkBuffer object)
	{
		DeferredResourceDestruction ret;
		ret.object.Buffer = object;
		ret.destroy_callback = [](VkDevice device, const Object& obj) { vkDestroyBuffer(device, obj.Buffer, nullptr); };
		return ret;
	}

	template<> static DeferredResourceDestruction Wrapper<VkBufferView>(VkBufferView object)
	{
		DeferredResourceDestruction ret;
		ret.object.BufferView = object;
		ret.destroy_callback = [](VkDevice device, const Object& obj) { vkDestroyBufferView(device, obj.BufferView, nullptr); };
		return ret;
	}

	template<> static DeferredResourceDestruction Wrapper<VkImage>(VkImage object)
	{
		DeferredResourceDestruction ret;
		ret.object.Image = object;
		ret.destroy_callback = [](VkDevice device, const Object& obj) { vkDestroyImage(device, obj.Image, nullptr); };
		return ret;
	}

	template<> static DeferredResourceDestruction Wrapper<VkImageView>(VkImageView object)
	{
		DeferredResourceDestruction ret;
		ret.object.ImageView = object;
		ret.destroy_callback = [](VkDevice device, const Object& obj) { vkDestroyImageView(device, obj.ImageView, nullptr); };
		return ret;
	}
};

}		// namespace Vulkan
