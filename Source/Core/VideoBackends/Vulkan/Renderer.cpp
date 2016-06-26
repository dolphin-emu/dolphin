// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StaticShaderCache.h"
#include "VideoBackends/Vulkan/Util.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan {

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

	// Initialize annoying statics
	s_last_efb_scale = g_ActiveConfig.iEFBScale;

	// Update backbuffer dimensions
	OnSwapChainResized();

	// Various initialization routines will have executed commands on the command buffer (which is currently the last one).
	// Execute what we have done before moving to the first buffer for the first frame.
	m_command_buffer_mgr->SubmitCommandBuffer(nullptr);
	BeginFrame();

	// Apply the default/initial state
	m_state_tracker->SetRenderPass(m_framebuffer_mgr->GetEFBRenderPass());
}

Renderer::~Renderer()
{
	g_Config.bRunning = false;
	UpdateActiveConfig();
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
	m_state_tracker->InvalidateDescriptorSets();
	RestoreAPIState();
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	VkClearAttachment clear_attachments[2];
	uint32_t num_clear_attachments = 0;

	// fast path: when both color and alpha are enabled, we can blow away the entire buffer
	// TODO: Can we also do it when the buffer is not an RGBA format?
	if (colorEnable && alphaEnable)
	{
		clear_attachments[num_clear_attachments].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		clear_attachments[num_clear_attachments].colorAttachment = 0;
		clear_attachments[num_clear_attachments].clearValue.color.float32[0] = float((color >> 16) & 0xFF) / 255.0f;
		clear_attachments[num_clear_attachments].clearValue.color.float32[1] = float((color >> 8) & 0xFF) / 255.0f;
		clear_attachments[num_clear_attachments].clearValue.color.float32[2] = float((color >> 0) & 0xFF) / 255.0f;
		clear_attachments[num_clear_attachments].clearValue.color.float32[3] = float((color >> 24) & 0xFF) / 255.0f;
		num_clear_attachments++;
	}
	else
	{
		// Mask away the appropriate colors and use a shader
		BlendState blend_state = Util::GetNoBlendingBlendState();
		if (colorEnable)
			blend_state.write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
		else
			blend_state.write_mask = VK_COLOR_COMPONENT_A_BIT;

		// No need to start a new render pass, but we do need to restore viewport state
		UtilityShaderDraw draw(m_object_cache, m_command_buffer_mgr, m_framebuffer_mgr->GetEFBRenderPass(),
			m_object_cache->GetStaticShaderCache().GetPassthroughVertexShader(),
			m_object_cache->GetStaticShaderCache().GetPassthroughGeometryShader(),
			m_object_cache->GetStaticShaderCache().GetClearFragmentShader());

		draw.DrawColoredQuad(0, 0, m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight(),
			float((color >> 16) & 0xFF) / 255.0f, float((color >> 8) & 0xFF) / 255.0f,
			float((color >> 0) & 0xFF) / 255.0f, float((color >> 24) & 0xFF) / 255.0f);

		m_state_tracker->SetPendingRebind();
	}

	if (zEnable)
	{
		clear_attachments[num_clear_attachments].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		clear_attachments[num_clear_attachments].colorAttachment = 0;
		clear_attachments[num_clear_attachments].clearValue.depthStencil.depth = 1.0f - (float(z & 0xFFFFFF) / 16777216.0f);
		clear_attachments[num_clear_attachments].clearValue.depthStencil.stencil = 0;
		num_clear_attachments++;
	}

	if (num_clear_attachments > 0)
	{
		// Native -> EFB coordinates
		TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);
		VkClearRect rect =
		{
			{																									// VkRect2D    rect
				{ targetRc.left, targetRc.top },																// VkOffset2D  offset
				{ static_cast<uint32_t>(targetRc.GetWidth()), static_cast<uint32_t>(targetRc.GetHeight()) }		// VkExtent2D  extent
			},
			0,																									// uint32_t    baseArrayLayer
			m_framebuffer_mgr->GetEFBLayers()																	// uint32_t    layerCount
		};
		vkCmdClearAttachments(m_command_buffer_mgr->GetCurrentCommandBuffer(), num_clear_attachments, clear_attachments, 1, &rect);
	}
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{

}

void Renderer::SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc, float gamma)
{
	ResetAPIState();

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

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

		// Blit the EFB to the back buffer (Swap chain)
		// TODO: XFB
		UtilityShaderDraw draw(
			m_object_cache, m_command_buffer_mgr, m_swap_chain->GetRenderPass(),
			m_object_cache->GetStaticShaderCache().GetPassthroughVertexShader(),
			nullptr,
			m_object_cache->GetStaticShaderCache().GetCopyFragmentShader());

		// Find the rect we want to draw into on the backbuffer
		TargetRectangle target_rc = GetTargetRectangle();
		TargetRectangle source_rc = Renderer::ConvertEFBRectangle(rc);

		draw.SetPSSampler(0, m_framebuffer_mgr->GetEFBColorTexture()->GetView(), m_object_cache->GetLinearSampler());

		draw.DrawQuad(source_rc.left, source_rc.top, source_rc.GetWidth(), source_rc.GetHeight(),
		              m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight(),
		              target_rc.left, target_rc.top, target_rc.GetWidth(), target_rc.GetHeight());

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

	m_state_tracker->SetPendingRebind();
}

void Renderer::SetGenerationMode()
{
	RasterizationState new_rs_state = {};

	switch (bpmem.genMode.cullmode)
	{
	case GenMode::CULL_NONE:	new_rs_state.cull_mode = VK_CULL_MODE_NONE;					break;
	case GenMode::CULL_BACK:	new_rs_state.cull_mode = VK_CULL_MODE_BACK_BIT;				break;
	case GenMode::CULL_FRONT:	new_rs_state.cull_mode = VK_CULL_MODE_FRONT_BIT;			break;
	case GenMode::CULL_ALL:		new_rs_state.cull_mode = VK_CULL_MODE_FRONT_AND_BACK;		break;
	default:					new_rs_state.cull_mode = VK_CULL_MODE_NONE;					break;
	}

	m_state_tracker->SetRasterizationState(new_rs_state);
}

void Renderer::SetDepthMode()
{
	DepthStencilState new_ds_state = {};
	new_ds_state.test_enable = (bpmem.zmode.testenable) ? VK_TRUE : VK_FALSE;
	new_ds_state.write_enable = (bpmem.zmode.updateenable) ? VK_TRUE : VK_FALSE;

	// Inverted depth, hence these are swapped
	switch (bpmem.zmode.func)
	{
	case ZMode::NEVER:			new_ds_state.compare_op = VK_COMPARE_OP_NEVER;				break;
	case ZMode::LESS:			new_ds_state.compare_op = VK_COMPARE_OP_GREATER;			break;
	case ZMode::EQUAL:			new_ds_state.compare_op = VK_COMPARE_OP_EQUAL;				break;
	case ZMode::LEQUAL:			new_ds_state.compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;	break;
	case ZMode::GREATER:		new_ds_state.compare_op = VK_COMPARE_OP_LESS;				break;
	case ZMode::NEQUAL:			new_ds_state.compare_op = VK_COMPARE_OP_NOT_EQUAL;			break;
	case ZMode::GEQUAL:			new_ds_state.compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;		break;
	case ZMode::ALWAYS:			new_ds_state.compare_op = VK_COMPARE_OP_ALWAYS;				break;
	default:					new_ds_state.compare_op = VK_COMPARE_OP_ALWAYS;				break;
	}

	m_state_tracker->SetDepthStencilState(new_ds_state);
}

void Renderer::SetColorMask()
{
	u32 color_mask = 0;

	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.alphaupdate && bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
			color_mask |= VK_COLOR_COMPONENT_A_BIT;
		if (bpmem.blendmode.colorupdate)
			color_mask |= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	}

	m_state_tracker->SetColorMask(color_mask);
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	BlendState new_blend_state = {};

	// Keep saved color mask
	new_blend_state.write_mask = m_state_tracker->GetColorWriteMask();

	// Fast path for blending disabled
	if (!bpmem.blendmode.blendenable)
	{
		new_blend_state.blend_enable = VK_FALSE;
		new_blend_state.blend_op = VK_BLEND_OP_ADD;
		new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;
		new_blend_state.dst_blend = VK_BLEND_FACTOR_ZERO;
		new_blend_state.use_dst_alpha = VK_FALSE;
		m_state_tracker->SetBlendState(new_blend_state);
		return;
	}
	// Fast path for subtract blending
	else if (bpmem.blendmode.subtract)
	{
		new_blend_state.blend_enable = VK_FALSE;
		new_blend_state.blend_op = VK_BLEND_OP_REVERSE_SUBTRACT;
		new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;
		new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE;
		new_blend_state.use_dst_alpha = VK_FALSE;
		m_state_tracker->SetBlendState(new_blend_state);
		return;
	}

	// Our render target always uses an alpha channel, so we need to override the blend functions to
	// assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel
	// is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;

	// TODO: Handle logic ops
	new_blend_state.blend_enable = VK_TRUE;
	new_blend_state.blend_op = VK_BLEND_OP_ADD;
	new_blend_state.use_dst_alpha = bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && target_has_alpha;

	switch (bpmem.blendmode.srcfactor)
	{
	case BlendMode::ZERO:		new_blend_state.src_blend = VK_BLEND_FACTOR_ZERO;					break;
	case BlendMode::ONE:		new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;					break;
	case BlendMode::DSTCLR:		new_blend_state.src_blend = VK_BLEND_FACTOR_DST_COLOR;				break;
	case BlendMode::INVDSTCLR:	new_blend_state.src_blend = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;	break;
	case BlendMode::SRCALPHA:	new_blend_state.src_blend = VK_BLEND_FACTOR_SRC_ALPHA;				break;
	case BlendMode::INVSRCALPHA:new_blend_state.src_blend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;	break;
	case BlendMode::DSTALPHA:	new_blend_state.src_blend = (target_has_alpha) ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_ONE;				break;
	case BlendMode::INVDSTALPHA:new_blend_state.src_blend = (target_has_alpha) ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ZERO;	break;
	default:					new_blend_state.src_blend = VK_BLEND_FACTOR_ONE;					break;
	}

	switch (bpmem.blendmode.dstfactor)
	{
	case BlendMode::ZERO:		new_blend_state.dst_blend = VK_BLEND_FACTOR_ZERO;					break;
	case BlendMode::ONE:		new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE;					break;
	case BlendMode::SRCCLR:		new_blend_state.dst_blend = VK_BLEND_FACTOR_SRC_COLOR;				break;
	case BlendMode::INVSRCCLR:	new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;	break;
	case BlendMode::SRCALPHA:	new_blend_state.dst_blend = VK_BLEND_FACTOR_SRC_ALPHA;				break;
	case BlendMode::INVSRCALPHA:new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;	break;
	case BlendMode::DSTALPHA:	new_blend_state.dst_blend = (target_has_alpha) ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_ONE;				break;
	case BlendMode::INVDSTALPHA:new_blend_state.dst_blend = (target_has_alpha) ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ZERO;	break;
	default:					new_blend_state.dst_blend = VK_BLEND_FACTOR_ONE;					break;
	}

	m_state_tracker->SetBlendState(new_blend_state);
}

void Renderer::SetLogicOpMode()
{
	// TODO: Implement me
}

void Renderer::SetSamplerState(int stage, int texindex, bool custom_tex)
{
	const FourTexUnits& tex = bpmem.tex[texindex];
	const TexMode0& tm0 = tex.texMode0[stage];
	const TexMode1& tm1 = tex.texMode1[stage];
	SamplerState new_state = {};

	if (texindex)
		stage += 4;

	// TODO: Anisotropic filtering

	if (g_ActiveConfig.bForceFiltering)
	{
		new_state.min_filter = VK_FILTER_LINEAR;
		new_state.mag_filter = VK_FILTER_LINEAR;
		new_state.mipmap_mode = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
	else
	{
		// Constants for these?
		new_state.min_filter = ((tm0.min_filter & 4) != 0) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		new_state.mipmap_mode = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		new_state.mag_filter = (tm0.mag_filter != 0) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	}

	// If mipmaps are disabled, clamp min/max lod
	new_state.max_lod = (SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0)) ? static_cast<u32>(MathUtil::Clamp(tm1.max_lod / 16.0f, 0.0f, 255.0f)) : 0;
	new_state.min_lod = std::min(new_state.max_lod.Value(), static_cast<u32>(MathUtil::Clamp(tm1.min_lod / 16.0f, 0.0f, 255.0f)));
	new_state.lod_bias = static_cast<s32>(tm0.lod_bias / 32.0f);

	// Custom textures may have a greater number of mips
	if (custom_tex)
		new_state.max_lod = 255;

	// Address modes
	static const VkSamplerAddressMode address_modes[] =
	{
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT
	};
	new_state.wrap_u = address_modes[tm0.wrap_s];
	new_state.wrap_v = address_modes[tm0.wrap_t];

	// Create a sampler if one matching this description does not already exist
	VkSampler sampler = m_object_cache->GetSampler(new_state);
	if (sampler == VK_NULL_HANDLE)
	{
		ERROR_LOG(VIDEO, "Failed to create sampler");
		sampler = m_object_cache->GetPointSampler();
	}

	m_state_tracker->SetPSSampler((texindex * 4) + stage, sampler);
}

void Renderer::SetDitherMode()
{
	// TODO: Implement me
}

void Renderer::SetInterlacingMode()
{
	// TODO: Implement me
}

void Renderer::SetScissorRect(const EFBRectangle& rc)
{
	TargetRectangle target_rc = ConvertEFBRectangle(rc);

	VkRect2D scissor = {
		{ target_rc.left, target_rc.top },
		{ static_cast<uint32_t>(target_rc.GetWidth()), static_cast<uint32_t>(target_rc.GetHeight()) }
	};

	m_state_tracker->SetScissor(scissor);
}

void Renderer::SetViewport()
{
	int scissor_x_offset = bpmem.scissorOffset.x * 2;
	int scissor_y_offset = bpmem.scissorOffset.y * 2;

	float x = Renderer::EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - scissor_x_offset);
	float y = Renderer::EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - scissor_y_offset);
	float width = Renderer::EFBToScaledXf(2.0f * xfmem.viewport.wd);
	float height = Renderer::EFBToScaledYf(-2.0f * xfmem.viewport.ht);
	if (width < 0.0f)
	{
		x += width;
		width = -width;
	}
	if (height < 0.0f)
	{
		y += height;
		height = -height;
	}

	// Use reversed depth here
	float min_depth = 1.0f - MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f;
	float max_depth = 1.0f - MathUtil::Clamp<float>(xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, 0.0f, 16777216.0f), 0.0f, 16777215.0f) / 16777216.0f;

	VkViewport viewport = { x, y, width, height, min_depth, max_depth };
	m_state_tracker->SetViewport(viewport);
}



} // namespace Vulkan
