// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Helpers.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Texture2D.h"

namespace Vulkan {

Texture2D::Texture2D(CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImageViewType view_type, VkImage image, VkDeviceMemory device_memory, VkImageView view, VkImageView depth_view)
	: m_command_buffer_mgr(command_buffer_mgr)
	, m_width(width)
	, m_height(height)
	, m_levels(levels)
	, m_layers(layers)
	, m_format(format)
	, m_layout(VK_IMAGE_LAYOUT_UNDEFINED)
	, m_view_type(view_type)
	, m_image(image)
	, m_device_memory(device_memory)
	, m_view(view)
	, m_depth_view(depth_view)
{

}

Texture2D::~Texture2D()
{
	m_command_buffer_mgr->DeferResourceDestruction(m_view);
	if (m_depth_view)
		m_command_buffer_mgr->DeferResourceDestruction(m_depth_view);

	// If we don't have device memory allocated, consider the image to be owned elsewhere (e.g. swapchain)
	if (m_device_memory)
	{
		m_command_buffer_mgr->DeferResourceDestruction(m_image);
		m_command_buffer_mgr->DeferResourceDestruction(m_device_memory);
	}
}

std::unique_ptr<Texture2D> Texture2D::Create(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImageViewType view_type, VkImageTiling tiling, VkImageUsageFlags usage)
{
	// Create image descriptor
	VkImageCreateInfo image_info = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		nullptr,
		0,
		VK_IMAGE_TYPE_2D,
		format,
		{ width, height, 1 },
		levels,
		layers,
		VK_SAMPLE_COUNT_1_BIT,		// TODO: MSAA
		tiling,
		usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		nullptr,
		VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkImage image = VK_NULL_HANDLE;
	VkResult res = vkCreateImage(object_cache->GetDevice(), &image_info, nullptr, &image);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateImage failed: ");
		return nullptr;
	}

	// Allocate memory to back this texture, we want device local memory in this case
	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(object_cache->GetDevice(), image, &memory_requirements);

	VkMemoryAllocateInfo memory_info = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		memory_requirements.size,
		object_cache->GetMemoryType(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};

	VkDeviceMemory device_memory;
	res = vkAllocateMemory(object_cache->GetDevice(), &memory_info, nullptr, &device_memory);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
		vkDestroyImage(object_cache->GetDevice(), image, nullptr);
		return nullptr;
	}

	res = vkBindImageMemory(object_cache->GetDevice(), image, device_memory, 0);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkBindImageMemory failed: ");
		vkDestroyImage(object_cache->GetDevice(), image, nullptr);
		vkFreeMemory(object_cache->GetDevice(), device_memory, nullptr);
		return nullptr;
	}

	VkImageViewCreateInfo view_info = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		nullptr,
		0,
		image,
		view_type,
		(IsDepthFormat(format)) ? GetColorFormatForDepthFormat(format) : format,
		{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, layers }
	};

	VkImageView view = VK_NULL_HANDLE;
	res = vkCreateImageView(object_cache->GetDevice(), &view_info, nullptr, &view);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
		vkDestroyImage(object_cache->GetDevice(), image, nullptr);
		vkFreeMemory(object_cache->GetDevice(), device_memory, nullptr);
		return nullptr;
	}

	// If it's a depth format, create the depth view as well
	VkImageView depth_view = VK_NULL_HANDLE;
	if (IsDepthFormat(format))
	{
		view_info.format = format;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		res = vkCreateImageView(object_cache->GetDevice(), &view_info, nullptr, &depth_view);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
			vkDestroyImageView(object_cache->GetDevice(), view, nullptr);
			vkDestroyImage(object_cache->GetDevice(), image, nullptr);
			vkFreeMemory(object_cache->GetDevice(), device_memory, nullptr);
			return nullptr;
		}
	}

	return std::make_unique<Texture2D>(command_buffer_mgr, width, height, levels, layers, format, view_type, image, device_memory, view, depth_view);
}

std::unique_ptr<Texture2D> Texture2D::CreateFromExistingImage(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImageViewType view_type, VkImage existing_image)
{
	// Only need to create the image view
	VkImageViewCreateInfo view_info = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		nullptr,
		0,
		existing_image,
		view_type,
		(IsDepthFormat(format)) ? GetColorFormatForDepthFormat(format) : format,
		{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, layers }
	};

	VkImageView view = VK_NULL_HANDLE;
	VkResult res = vkCreateImageView(object_cache->GetDevice(), &view_info, nullptr, &view);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
		return nullptr;
	}

	// If it's a depth format, create the depth view as well
	VkImageView depth_view = VK_NULL_HANDLE;
	if (IsDepthFormat(format))
	{
		view_info.format = format;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		res = vkCreateImageView(object_cache->GetDevice(), &view_info, nullptr, &depth_view);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
			vkDestroyImageView(object_cache->GetDevice(), view, nullptr);
			return nullptr;
		}
	}

	return std::make_unique<Texture2D>(command_buffer_mgr, width, height, levels, layers, format, view_type, existing_image, nullptr, view, depth_view);
}

void Texture2D::OverrideImageLayout(VkImageLayout new_layout)
{
	m_layout = new_layout;
}

void Texture2D::TransitionToLayout(VkCommandBuffer command_buffer, VkImageLayout new_layout)
{
	if (m_layout == new_layout)
		return;

	VkImageMemoryBarrier barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType            sType
		nullptr,											// const void*                pNext
		0,													// VkAccessFlags              srcAccessMask
		0,													// VkAccessFlags              dstAccessMask
		m_layout,											// VkImageLayout              oldLayout
		new_layout,											// VkImageLayout              newLayout
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t                   srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t                   dstQueueFamilyIndex
		m_image,											// VkImage                    image
		{ static_cast<VkImageAspectFlags>(IsDepthFormat(m_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, m_levels, 0, m_layers }	// VkImageSubresourceRange    subresourceRange
	};

	switch (m_layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:							barrier.srcAccessMask = 0;												break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:	barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;	break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;						break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;					break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;					break;
	default:																														break;
	}

	switch (new_layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:							barrier.dstAccessMask = 0;												break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:	barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;	break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;						break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;					break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;					break;
	default:																														break;
	}

	// TODO: Better selection of srcStageMask/dstStageMask
	vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrier);
}

}		// namespace Vulkan
