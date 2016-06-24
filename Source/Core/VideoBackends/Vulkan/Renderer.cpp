// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>

#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"

#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/PixelShaderManager.h"

namespace Vulkan
{

// Init functions
Renderer::Renderer(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, SwapChain* swap_chain, StateTracker* state_tracker)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
	, m_swap_chain(swap_chain)
	, m_state_tracker(state_tracker)
{
	g_Config.bRunning = true;
	UpdateActiveConfig();

	// Work around the stupid static crap
	m_framebuffer_mgr = static_cast<FramebufferManager*>(g_framebuffer_manager.get());

	if (!CreateSemaphores())
		PanicAlert("Failed to create Renderer semaphores");

	// Update backbuffer dimensions
	OnSwapChainResized();

	// Various initialization routines will have executed commands on the command buffer (which is currently the last one).
	// Execute what we have done before moving to the first buffer for the first frame.
	m_command_buffer_mgr->SubmitCommandBuffer(nullptr);
	BeginFrame();
}

Renderer::~Renderer()
{
	g_Config.bRunning = false;
	UpdateActiveConfig();

	// Submit command list before closing, but skip presenting
	Renderer::ResetAPIState();
	m_command_buffer_mgr->ExecuteCommandBuffer(true);
	DestroySemaphores();
}

bool Renderer::CreateSemaphores()
{
	// Create two semaphores, one that is triggered when the swapchain buffer is ready, another after submit and before present
	VkSemaphoreCreateInfo semaphore_info = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,      // VkStructureType          sType
		nullptr,                                      // const void*              pNext
		0                                             // VkSemaphoreCreateFlags   flags
	};

	VkResult res;
	if ((res = vkCreateSemaphore(m_object_cache->GetDevice(), &semaphore_info, nullptr, &m_image_available_semaphore)) != VK_SUCCESS ||
		(res = vkCreateSemaphore(m_object_cache->GetDevice(), &semaphore_info, nullptr, &m_rendering_finished_semaphore)) != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateSemaphore failed: ");
		return false;
	}

	return true;
}

void Renderer::DestroySemaphores()
{
	if (m_image_available_semaphore)
	{
		vkDestroySemaphore(m_object_cache->GetDevice(), m_image_available_semaphore, nullptr);
		m_image_available_semaphore = nullptr;
	}

	if (m_rendering_finished_semaphore)
	{
		vkDestroySemaphore(m_object_cache->GetDevice(), m_rendering_finished_semaphore, nullptr);
		m_rendering_finished_semaphore = nullptr;
	}
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
	printf("RenderText: %s\n", text.c_str());
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = rc.left;
	result.top    = rc.top;
	result.right  = rc.right;
	result.bottom = rc.bottom;
	return result;
}

void Renderer::BeginFrame()
{
	// Grab the next image from the swap chain
	if (!m_swap_chain->AcquireNextImage(m_image_available_semaphore))
		PanicAlert("Failed to grab image from swap chain");

	// Activate a new command list, and restore state ready for the next draw
	m_command_buffer_mgr->ActivateCommandBuffer(m_image_available_semaphore);
	RestoreAPIState();
}

void Renderer::SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc, float gamma)
{
	ResetAPIState();

	// Blitting to the screen
	{
		// Transition from present to attachment so we can write to it
		m_swap_chain->TransitionToAttachment(m_command_buffer_mgr->GetCurrentCommandBuffer());

		// Begin the present render pass
		VkClearValue clear_value = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		VkRenderPassBeginInfo begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			m_swap_chain->GetRenderPass(),
			m_swap_chain->GetCurrentFramebuffer(),
			{ { 0, 0 }, m_swap_chain->GetSize() },
			1,
			&clear_value
		};
		vkCmdBeginRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);

		// Draw stuff

		// End the present render pass
		vkCmdEndRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer());

		// Transition back to present source so we can display it
		m_swap_chain->TransitionToPresent(m_command_buffer_mgr->GetCurrentCommandBuffer());
	}

	// Submit the current command buffer, signaling rendering finished semaphore when it's done
	m_command_buffer_mgr->SubmitCommandBuffer(m_rendering_finished_semaphore);

	// Queue a present of the swap chain
	m_swap_chain->Present(m_rendering_finished_semaphore);

	UpdateActiveConfig();

	// Prep for the next frame
	BeginFrame();
}

void Renderer::OnSwapChainResized()
{
	s_backbuffer_width = m_swap_chain->GetSize().width;
	s_backbuffer_height = m_swap_chain->GetSize().height;
	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);
	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
	if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height))
		m_framebuffer_mgr->ResizeEFBTextures();

	PixelShaderManager::SetEfbScaleChanged();
}

void Renderer::ApplyState(bool bUseDstAlpha)
{
	m_state_tracker->CheckForShaderChanges(0, bUseDstAlpha ? DSTALPHA_DUAL_SOURCE_BLEND : DSTALPHA_NONE);
}

void Renderer::RestoreState()
{
	m_state_tracker->Bind(m_command_buffer_mgr->GetCurrentCommandBuffer(), true);
}

void Renderer::ResetAPIState()
{
	// End the EFB render pass
	vkCmdEndRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer());
}

void Renderer::RestoreAPIState()
{
	// Restart EFB render pass
	VkRenderPassBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		nullptr,
		m_framebuffer_mgr->GetEFBRenderPass(),
		m_framebuffer_mgr->GetEFBFramebuffer(),
		{ { 0, 0 }, { m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight() } },
		0,
		nullptr
	};

	vkCmdBeginRenderPass(m_command_buffer_mgr->GetCurrentCommandBuffer(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);

	// TODO: Restore viewport
	// TODO: Restore shaders
}

} // namespace Vulkan
