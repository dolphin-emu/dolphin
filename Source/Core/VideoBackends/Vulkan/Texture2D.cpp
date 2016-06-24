#include <algorithm>
#include <cassert>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Helpers.h"
#include "VideoBackends/Vulkan/ObjectCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"

namespace Vulkan {

Texture2D::Texture2D(CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImage image, VkDeviceMemory device_memory, VkImageView view)
	: m_command_buffer_mgr(command_buffer_mgr)
	, m_width(width)
	, m_height(height)
	, m_levels(levels)
	, m_layers(layers)
	, m_format(format)
	, m_layout(VK_IMAGE_LAYOUT_UNDEFINED)
	, m_image(image)
	, m_device_memory(device_memory)
	, m_view(view)
{

}

Texture2D::~Texture2D()
{
	if (m_view)
		m_command_buffer_mgr->DeferResourceDestruction(m_view);
	if (m_image)
		m_command_buffer_mgr->DeferResourceDestruction(m_image);
}

std::unique_ptr<Texture2D> Texture2D::Create(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
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
		return false;
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
		return false;
	}

	// Bind the allocated memory to the image
	res = vkBindImageMemory(object_cache->GetDevice(), image, device_memory, 0);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkBindImageMemory failed: ");
		vkFreeMemory(object_cache->GetDevice(), device_memory, nullptr);
		vkDestroyImage(object_cache->GetDevice(), image, nullptr);
		return false;
	}

	// Create view of this image
	VkImageViewCreateInfo view_info = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		nullptr,
		0,
		image,
		VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		format,
		{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
		{ static_cast<VkImageAspectFlags>(IsDepthFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, levels, 0, layers }
	};

	VkImageView view = VK_NULL_HANDLE;
	res = vkCreateImageView(object_cache->GetDevice(), &view_info, nullptr, &view);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
		vkFreeMemory(object_cache->GetDevice(), device_memory, nullptr);
		vkDestroyImage(object_cache->GetDevice(), image, nullptr);
		return false;
	}

	return std::make_unique<Texture2D>(command_buffer_mgr, width, height, levels, layers, format, image, device_memory, view);	
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
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:			barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;						break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;					break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;					break;
	}

	switch (new_layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:							barrier.dstAccessMask = 0;												break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:	barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;	break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:			barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;						break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;					break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;					break;
	}

	vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrier);
}

}		// namespace Vulkan
