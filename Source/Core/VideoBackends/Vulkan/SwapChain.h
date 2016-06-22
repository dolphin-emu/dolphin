// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <memory>
#include <vector>

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/Globals.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

class ObjectCache;

class SwapChain
{
public:
	SwapChain() = default;
	~SwapChain();

	bool Initialize(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkQueue present_queue, VkCommandBuffer setup_command_buffer);

	VkSurfaceKHR GetSurface() const { return m_surface; }
	VkSurfaceFormatKHR GetSurfaceFormat() const { return m_surface_format; }
	VkSwapchainKHR GetSwapChain() const { return m_swap_chain; }
	VkRenderPass GetRenderPass() const { return m_render_pass; }
	VkExtent2D GetSize() const { return m_size; }

	VkImage GetCurrentImage() const { return m_swap_chain_images[m_current_swap_chain_image_index].Image; }
	VkFramebuffer GetCurrentFramebuffer() const { return m_swap_chain_images[m_current_swap_chain_image_index].Framebuffer; }

	bool AcquireNextImage(VkSemaphore available_semaphore);
	void TransitionToAttachment(VkCommandBuffer command_buffer);
	void TransitionToPresent(VkCommandBuffer command_buffer);
	bool Present(VkSemaphore rendering_complete_semaphore);

private:
	bool SelectFormats();
	
	bool CreateSwapChain(VkSwapchainKHR old_swap_chain);
	void DestroySwapChain();

	bool CreateRenderPass();
	void DestroyRenderPass();

	bool SetupSwapChainImages(VkCommandBuffer setup_command_buffer);
	void DestroySwapChainImages();

	struct SwapChainImage
	{
		VkImage Image;
		VkImageView View;
		VkFramebuffer Framebuffer;
	};

	VkPhysicalDevice m_physical_device = nullptr;
	VkDevice m_device = nullptr;
	VkSurfaceKHR m_surface = nullptr;
	VkSurfaceFormatKHR m_surface_format;

	VkQueue m_present_queue = nullptr;
	
	VkSwapchainKHR m_swap_chain = nullptr;
	std::vector<SwapChainImage> m_swap_chain_images;
	uint32_t m_current_swap_chain_image_index = 0;

	VkRenderPass m_render_pass = nullptr;

	VkExtent2D m_size = { 0, 0 };

};

}  // namespace Vulkan
