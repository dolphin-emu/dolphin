// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>

#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/Helpers.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"

namespace Vulkan {

SwapChain::~SwapChain()
{
	DestroySwapChainImages();
	DestroySwapChain();
	DestroyRenderPass();
}

bool SwapChain::Initialize(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkQueue present_queue, VkCommandBuffer setup_command_buffer)
{
	m_physical_device = physical_device;
	m_device = device;
	m_surface = surface;
	m_present_queue = present_queue;

	if (!SelectFormats())
		return false;

	if (!CreateRenderPass())
		return false;

	if (!CreateSwapChain(nullptr))
		return false;

	if (!SetupSwapChainImages(setup_command_buffer))
		return false;

	return true;
}

bool SwapChain::SelectFormats()
{
	// Select swap chain format
	m_surface_format = SelectVulkanSurfaceFormat(m_physical_device, m_surface);
	if (m_surface_format.format == VK_FORMAT_RANGE_SIZE)
		return false;

	return true;
}

bool SwapChain::CreateRenderPass()
{
	// render pass for rendering to the swap chain
	VkAttachmentDescription present_render_pass_attachments[] = {
		{
			0,
			m_surface_format.format,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		}
	};

	VkAttachmentReference present_render_pass_color_attachment_references[] = {
		{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
	};

	VkSubpassDescription present_render_pass_subpass_descriptions[] = {
		{ 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, present_render_pass_color_attachment_references, nullptr, nullptr, 0, nullptr }
	};

	VkRenderPassCreateInfo present_render_pass_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		_countof(present_render_pass_attachments),
		present_render_pass_attachments,
		_countof(present_render_pass_subpass_descriptions),
		present_render_pass_subpass_descriptions,
		0,
		nullptr
	};

	VkResult res = vkCreateRenderPass(m_device, &present_render_pass_info, nullptr, &m_render_pass);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateRenderPass (present) failed: ");
		return false;
	}

	return true;
}

void SwapChain::DestroyRenderPass()
{
	if (m_render_pass)
	{
		vkDestroyRenderPass(m_device, m_render_pass, nullptr);
		m_render_pass = nullptr;
	}
}

bool SwapChain::CreateSwapChain(VkSwapchainKHR old_swap_chain)
{
	// Look up surface properties to determine image count and dimensions
	VkSurfaceCapabilitiesKHR surface_capabilities;
	VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilities);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: ");
		return false;
	}

	// Select swap chain format and present mode
	VkPresentModeKHR present_mode = SelectVulkanPresentMode(m_physical_device, m_surface);
	if (present_mode == VK_PRESENT_MODE_RANGE_SIZE_KHR)
		return false;

	// Select number of images in swap chain, we prefer one buffer in the background to work on
	uint32_t image_count = std::min(surface_capabilities.minImageCount + 1, surface_capabilities.maxImageCount);

	// Determine the dimensions of the swap chain. Values of -1 indicate the size we specify here determines window size?
	m_size = surface_capabilities.currentExtent;
	if (m_size.width == -1)
	{
		m_size.width = std::min(std::max(surface_capabilities.minImageExtent.width, 640u), surface_capabilities.maxImageExtent.width);
		m_size.height = std::min(std::max(surface_capabilities.minImageExtent.height, 480u), surface_capabilities.maxImageExtent.height);
	}
	
	// Prefer identity transform if possible
	VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	if (!(surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR))
		transform = surface_capabilities.currentTransform;

	// Select swap chain flags, we only need a colour attachment
	VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (!(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
	{
		ERROR_LOG(VIDEO, "Vulkan: Swap chain does not support usage as color attachment");
		return false;
	}

	// Now we can actually create the swap chain
	VkSwapchainCreateInfoKHR swap_chain_info = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr,
		0,
		m_surface,
		image_count,
		m_surface_format.format,
		m_surface_format.colorSpace,
		m_size,
		1,
		image_usage,
		/*(m_graphics_queue_family_index != m_present_queue_family_index) ? VK_SHARING_MODE_CONCURRENT : */VK_SHARING_MODE_EXCLUSIVE,
		0,
		nullptr,
		transform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		present_mode,
		VK_TRUE,
		old_swap_chain
	};

	res = vkCreateSwapchainKHR(m_device, &swap_chain_info, nullptr, &m_swap_chain);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateCommandPool failed: ");
		return false;
	}

	// now destroy the old swap chain, since it's been recreated
	if (old_swap_chain)
		vkDestroySwapchainKHR(m_device, old_swap_chain, nullptr);

	return true;
}

bool SwapChain::SetupSwapChainImages(VkCommandBuffer setup_command_buffer)
{
	assert(m_swap_chain_images.empty());

	uint32_t image_count;
	VkResult res = vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, nullptr);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkGetSwapchainImagesKHR failed: ");
		return false;
	}

	std::vector<VkImage> images(image_count);
	res = vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, images.data());
	assert(res == VK_SUCCESS);

	m_swap_chain_images.reserve(image_count);
	for (uint32_t i = 0; i < image_count; i++)
	{
		SwapChainImage image;
		image.Image = images[i];

		VkImageViewCreateInfo view_info = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			nullptr,
			0,
			images[i],
			VK_IMAGE_VIEW_TYPE_2D,
			m_surface_format.format,
			{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		res = vkCreateImageView(m_device, &view_info, nullptr, &image.View);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
			return false;
		}

		VkFramebufferCreateInfo framebuffer_info = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr,
			0,
			m_render_pass,
			1,
			&image.View,
			m_size.width,
			m_size.height,
			1
		};

		res = vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &image.Framebuffer);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
			vkDestroyImageView(m_device, image.View, nullptr);
			return false;
		}

		// Transition this image from undefined to present src as expected
		// TODO: Maybe we should just wrap these in our Texture2D class?
		VkImageMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType            sType
			nullptr,											// const void*                pNext
			0,													// VkAccessFlags              srcAccessMask
			VK_ACCESS_MEMORY_READ_BIT,							// VkAccessFlags              dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout              oldLayout
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,					// VkImageLayout              newLayout
			VK_QUEUE_FAMILY_IGNORED,							// uint32_t                   srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,							// uint32_t                   dstQueueFamilyIndex
			image.Image,										// VkImage                    image
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }			// VkImageSubresourceRange    subresourceRange
		};

		vkCmdPipelineBarrier(setup_command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		m_swap_chain_images.push_back(image);
	}

	return true;
}

void SwapChain::DestroySwapChainImages()
{
	for (auto& it : m_swap_chain_images)
	{
		// Images themselves are cleaned up by the swap chain object
		vkDestroyFramebuffer(m_device, it.Framebuffer, nullptr);
		vkDestroyImageView(m_device, it.View, nullptr);
	}
	m_swap_chain_images.clear();
}

void SwapChain::DestroySwapChain()
{
	DestroySwapChainImages();
	vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
	m_swap_chain = nullptr;
}

bool SwapChain::AcquireNextImage(VkSemaphore available_semaphore)
{
	VkResult res = vkAcquireNextImageKHR(m_device, m_swap_chain, UINT64_MAX, available_semaphore, nullptr, &m_current_swap_chain_image_index);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkAcquireNextImageKHR failed: ");
		return false;
	}

	return true;
}

void SwapChain::TransitionToAttachment(VkCommandBuffer command_buffer)
{
	// Transition from present to color attachment
	VkImageMemoryBarrier barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		m_swap_chain_images[m_current_swap_chain_image_index].Image,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void SwapChain::TransitionToPresent(VkCommandBuffer command_buffer)
{
	// Transition from color attachment to present to color attachment
	VkImageMemoryBarrier barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		m_swap_chain_images[m_current_swap_chain_image_index].Image,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool SwapChain::Present(VkSemaphore rendering_complete_semaphore)
{
	VkPresentInfoKHR present_info = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		nullptr,
		1,
		&rendering_complete_semaphore,
		1,
		&m_swap_chain,
		&m_current_swap_chain_image_index,
		nullptr
	};

	VkResult res = vkQueuePresentKHR(m_present_queue, &present_info);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkQueuePresentKHR failed: ");
		return false;
	}

	return true;
}

}
