// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Helpers.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"

#include "VideoCommon/RenderBase.h"

namespace Vulkan {

FramebufferManager::FramebufferManager(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
{
	if (!CreateEFBRenderPass())
		PanicAlert("Failed to create EFB render pass");
	if (!CreateEFBFramebuffer())
		PanicAlert("Failed to create EFB textures");
}

FramebufferManager::~FramebufferManager()
{
	DestroyEFBFramebuffer();
	DestroyEFBRenderPass();
}

void FramebufferManager::GetTargetSize(unsigned int *width, unsigned int *height)
{
    *width = m_efb_width;
    *height = m_efb_height;
}

bool FramebufferManager::CreateEFBRenderPass()
{
    // render pass for rendering to the efb
	VkAttachmentDescription attachments[] = {
		{
			0,
			EFB_COLOR_TEXTURE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,		// TODO: MSAA
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		},
		{
			0,
			EFB_DEPTH_TEXTURE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		}
	};

	VkAttachmentReference color_attachment_references[] = {
		{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
	};

	VkAttachmentReference depth_attachment_reference = {
		1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass_descriptions[] = {
		{
			0, VK_PIPELINE_BIND_POINT_GRAPHICS,
			0, nullptr,
			1, color_attachment_references,
			nullptr,
			&depth_attachment_reference,
			0, nullptr }
	};

	VkRenderPassCreateInfo pass_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		ARRAYSIZE(attachments),
		attachments,
		ARRAYSIZE(subpass_descriptions),
		subpass_descriptions,
		0,
		nullptr
	};

	VkResult res = vkCreateRenderPass(m_object_cache->GetDevice(), &pass_info, nullptr, &m_efb_render_pass);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateRenderPass (EFB) failed: ");
		return false;
	}

	return true;
}

void FramebufferManager::DestroyEFBRenderPass()
{
	if (m_efb_render_pass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(m_object_cache->GetDevice(), m_efb_render_pass, nullptr);
		m_efb_render_pass = VK_NULL_HANDLE;
	}
}


bool FramebufferManager::CreateEFBFramebuffer()
{
	m_efb_width = static_cast<u32>(std::max(Renderer::GetTargetWidth(), 1));
	m_efb_height = static_cast<u32>(std::max(Renderer::GetTargetHeight(), 1));
	m_efb_layers = 1;
	INFO_LOG(VIDEO, "EFB size: %ux%ux%u", m_efb_width, m_efb_height, m_efb_layers);

	// TODO: Stereo buffers

	m_efb_color_texture = Texture2D::Create(m_object_cache, m_command_buffer_mgr,
		m_efb_width, m_efb_height, 1, m_efb_layers,
		EFB_COLOR_TEXTURE_FORMAT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	m_efb_depth_texture = Texture2D::Create(m_object_cache, m_command_buffer_mgr,
		m_efb_width, m_efb_height, 1, m_efb_layers,
		EFB_DEPTH_TEXTURE_FORMAT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	if (!m_efb_color_texture || !m_efb_depth_texture)
		return false;

	VkImageView framebuffer_attachments[] = {
		m_efb_color_texture->GetView(),
		m_efb_depth_texture->GetDepthView(),
	};

	VkFramebufferCreateInfo framebuffer_info = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		m_efb_render_pass,
		ARRAYSIZE(framebuffer_attachments),
		framebuffer_attachments,
		m_efb_width,
		m_efb_height,
		m_efb_layers
	};

	VkResult res = vkCreateFramebuffer(m_object_cache->GetDevice(), &framebuffer_info, nullptr, &m_efb_framebuffer);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
		return false;
	}

	// Transition to state that can be used to clear
	m_efb_color_texture->TransitionToLayout(m_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	m_efb_depth_texture->TransitionToLayout(m_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Clear the contents of the buffers.
	// TODO: On a resize, this should really be copying the old contents in.
	static const VkClearColorValue clear_color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	static const VkClearDepthStencilValue clear_depth = { 1.0f, 0 };
	VkImageSubresourceRange clear_color_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, m_efb_layers };
	VkImageSubresourceRange clear_depth_range = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, m_efb_layers };
	vkCmdClearColorImage(m_command_buffer_mgr->GetCurrentCommandBuffer(), m_efb_color_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &clear_color_range);
	vkCmdClearDepthStencilImage(m_command_buffer_mgr->GetCurrentCommandBuffer(), m_efb_depth_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_depth, 1, &clear_depth_range);

	// Transition ready for rendering
	m_efb_color_texture->TransitionToLayout(m_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_efb_depth_texture->TransitionToLayout(m_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	return true;
}

void FramebufferManager::DestroyEFBFramebuffer()
{
	if (m_efb_framebuffer != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(m_object_cache->GetDevice(), m_efb_framebuffer, nullptr);
		m_efb_framebuffer = VK_NULL_HANDLE;
	}

	m_efb_color_texture.reset();
	m_efb_depth_texture.reset();
}

void FramebufferManager::ResizeEFBTextures()
{
	DestroyEFBFramebuffer();
	if (!CreateEFBFramebuffer())
		PanicAlert("Failed to create EFB textures");
}

}		// namespace Vulkan
