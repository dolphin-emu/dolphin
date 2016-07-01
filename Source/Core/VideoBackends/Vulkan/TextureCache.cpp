// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <cstring>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PaletteTextureConverter.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/TextureEncoder.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

TextureCache::TextureCache(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
	, m_state_tracker(state_tracker)
{
	m_texture_upload_buffer = StreamBuffer::Create(object_cache, command_buffer_mgr, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE, MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE);
	if (!m_texture_upload_buffer)
		PanicAlert("Failed to create texture upload buffer");

	if (!CreateCopyRenderPass())
		PanicAlert("Failed to create copy render pass");

	m_texture_encoder = std::make_unique<TextureEncoder>(object_cache, command_buffer_mgr, state_tracker);

  m_palette_texture_converter = std::make_unique<PaletteTextureConverter>(object_cache, command_buffer_mgr, state_tracker);
  if (!m_palette_texture_converter->Initialize())
    PanicAlert("Failed to initialize palette texture converter");
}

TextureCache::~TextureCache()
{
	if (m_copy_render_pass != VK_NULL_HANDLE)
		m_command_buffer_mgr->DeferResourceDestruction(m_copy_render_pass);
}

void TextureCache::CompileShaders()
{

}

void TextureCache::DeleteShaders()
{

}

void TextureCache::ConvertTexture(TCacheEntryBase* base_entry, TCacheEntryBase* base_unconverted, void* palette, TlutFormat format)
{
  TCacheEntry* entry = static_cast<TCacheEntry*>(base_entry);
  TCacheEntry* unconverted = static_cast<TCacheEntry*>(base_unconverted);
  assert(entry->config.rendertarget);

  m_palette_texture_converter->ConvertTexture(entry->GetTexture(),
                                              entry->GetFramebuffer(),
                                              unconverted->GetTexture(),
                                              entry->config.width,
                                              entry->config.height,
                                              palette,
                                              format);

}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
						   PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
						   bool is_intensity, bool scale_by_half)
{
	// A better way of doing this would be nice.
	FramebufferManager* framebuffer_mgr = static_cast<FramebufferManager*>(g_framebuffer_manager.get());

	// TODO: MSAA case where we need to resolve first.
	VkImageView src_view = (src_format == PEControl::Z24) ?
								framebuffer_mgr->GetEFBDepthTexture()->GetView() :
								framebuffer_mgr->GetEFBColorTexture()->GetView();

	m_texture_encoder->EncodeTextureToRam(src_view, dst, format, native_width, bytes_per_row,
										  num_blocks_y, memory_stride, src_format, is_intensity,
										  scale_by_half, src_rect);
}

TextureCacheBase::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	// Allocate texture object
	std::unique_ptr<Texture2D> texture = Texture2D::Create(m_object_cache, m_command_buffer_mgr,
		config.width, config.height, config.levels, config.layers, TEXTURECACHE_TEXTURE_FORMAT,
		VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	if (!texture)
		return nullptr;

	// If this is a render target (For efb copies), allocate a framebuffer
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	if (config.rendertarget)
	{
		VkImageView framebuffer_attachments[] = { texture->GetView() };
		VkFramebufferCreateInfo framebuffer_info =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr,
			0,
			m_copy_render_pass,
			ARRAYSIZE(framebuffer_attachments),
			framebuffer_attachments,
			texture->GetWidth(),
			texture->GetHeight(),
			texture->GetLayers()
		};

		VkResult res = vkCreateFramebuffer(m_object_cache->GetDevice(), &framebuffer_info, nullptr, &framebuffer);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
			return nullptr;
		}
	}

	return new TCacheEntry(config, this, std::move(texture), framebuffer);
}

bool TextureCache::CreateCopyRenderPass()
{
	VkAttachmentDescription attachments[] = {
		{
			0,
			TEXTURECACHE_TEXTURE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,		// TODO: MSAA
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		}
	};

	VkAttachmentReference color_attachment_references[] =
	{
		{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
	};

	VkSubpassDescription subpass_descriptions[] =
	{
		{
			0, VK_PIPELINE_BIND_POINT_GRAPHICS,
			0, nullptr,
			1, color_attachment_references,
			nullptr,
			nullptr,
			0, nullptr
		}
	};

	VkRenderPassCreateInfo pass_info =
	{
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

	VkResult res = vkCreateRenderPass(m_object_cache->GetDevice(), &pass_info, nullptr, &m_copy_render_pass);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateRenderPass (Copy) failed: ");
		return false;
	}

	return true;
}

TextureCache::TCacheEntry::TCacheEntry(const TCacheEntryConfig& _config, TextureCache* parent, std::unique_ptr<Texture2D> texture, VkFramebuffer framebuffer)
	: TCacheEntryBase(_config)
	, m_parent(parent)
	, m_texture(std::move(texture))
	, m_framebuffer(framebuffer)
{

}

TextureCache::TCacheEntry::~TCacheEntry()
{
	// Texture is automatically cleaned up, however, we don't want to leave it bound to the state tracker.
	m_parent->m_state_tracker->UnbindTexture(m_texture->GetView());

	if (m_framebuffer != VK_NULL_HANDLE)
		m_parent->m_command_buffer_mgr->DeferResourceDestruction(m_framebuffer);
}

void TextureCache::TCacheEntry::Load(unsigned int width, unsigned int height, unsigned int expanded_width, unsigned int level)
{
	ObjectCache* object_cache = m_parent->m_object_cache;
	CommandBufferManager* command_buffer_mgr = m_parent->m_command_buffer_mgr;
	StreamBuffer* upload_buffer = m_parent->m_texture_upload_buffer.get();

	// Can't copy data larger than the texture extents.
	width = std::min(width, m_texture->GetWidth());
	height = std::min(height, m_texture->GetHeight());

	// Determine optimal row pitch, since we're going to be copying anyway.
	VkDeviceSize upload_width = Util::AlignValue(width, object_cache->GetTextureUploadPitchAlignment());
	VkDeviceSize upload_pitch = upload_width * sizeof(u32);
	VkDeviceSize upload_size = upload_pitch * height;

	// Allocate memory from the streaming buffer for the texture data.
	// TODO: Handle cases where the texture does not fit into the streaming buffer, we need to allocate a temporary buffer.
	if (!m_parent->m_texture_upload_buffer->ReserveMemory(upload_size, object_cache->GetTextureUploadAlignment()))
	{
		// Execute the command buffer first.
		Util::ExecuteCurrentCommandsAndRestoreState(command_buffer_mgr, m_parent->m_state_tracker);

		// Try allocating again. This may cause a fence wait.
		if (!upload_buffer->ReserveMemory(upload_size, object_cache->GetTextureUploadAlignment()))
			PanicAlert("Failed to allocate space in texture upload buffer");
	}

	// Grab buffer pointers
	VkBuffer image_upload_buffer = upload_buffer->GetBuffer();
	VkDeviceSize image_upload_buffer_offset = upload_buffer->GetCurrentOffset();
	u8* image_upload_buffer_pointer = upload_buffer->GetCurrentHostPointer();

	// Copy to the buffer using the stride from the subresource layout
	VkDeviceSize source_pitch = expanded_width * 4;
	const u8* source_ptr = TextureCache::temp;
	if (upload_pitch != source_pitch)
	{
		VkDeviceSize copy_pitch = std::min(source_pitch, upload_pitch);
		for (unsigned int row = 0; row < height; row++)
			memcpy(image_upload_buffer_pointer + row * upload_pitch, source_ptr + row * source_pitch, copy_pitch);
	}
	else
	{
		// Can copy the whole thing in one block, the pitch matches
		memcpy(image_upload_buffer_pointer, source_ptr, upload_size);
	}

	// Flush buffer memory if necessary
	upload_buffer->CommitMemory(upload_size);

	// Copy from the streaming buffer to the actual image.
	VkBufferImageCopy image_copy =
	{
		image_upload_buffer_offset,								// VkDeviceSize                bufferOffset
		static_cast<uint32_t>(upload_width),					// uint32_t                    bufferRowLength
		height,													// uint32_t                    bufferImageHeight
		{ VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 },				// VkImageSubresourceLayers    imageSubresource
		{ 0, 0, 0 },											// VkOffset3D                  imageOffset
		{ width, height, 1 }									// VkExtent3D                  imageExtent
	};

	// We can't execute texture uploads inside a render pass.
	m_parent->m_state_tracker->EndRenderPass();

	// Transition the texture to a transfer destination, invoke the transfer, then transition back.
	// TODO: Only transition the layer we're copying to.
	m_texture->TransitionToLayout(command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdCopyBufferToImage(command_buffer_mgr->GetCurrentCommandBuffer(), image_upload_buffer, m_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);
	m_texture->TransitionToLayout(command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& src_rect, bool scale_by_half, unsigned int cbufid, const float* colmat)
{
	// A better way of doing this would be nice.
	FramebufferManager* framebuffer_mgr = static_cast<FramebufferManager*>(g_framebuffer_manager.get());
	TargetRectangle scaled_src_rect = g_renderer->ConvertEFBRectangle(src_rect);
	bool is_depth_copy = (src_format == PEControl::Z24);

	// Has to be flagged as a render target.
	assert(m_framebuffer != VK_NULL_HANDLE);

	// Can't be done in a render pass, since we're doing our own render pass!
	m_parent->m_state_tracker->EndRenderPass();

	ObjectCache* object_cache = m_parent->m_object_cache;
	CommandBufferManager* command_buffer_mgr = m_parent->m_command_buffer_mgr;
	VkCommandBuffer command_buffer = command_buffer_mgr->GetCurrentCommandBuffer();
	m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	UtilityShaderDraw draw(object_cache,
						   command_buffer_mgr,
						   m_parent->m_copy_render_pass,
						   object_cache->GetStaticShaderCache().GetPassthroughVertexShader(),
						   object_cache->GetStaticShaderCache().GetPassthroughGeometryShader(),
						   is_depth_copy ?
								object_cache->GetStaticShaderCache().GetDepthMatrixFragmentShader() :
								object_cache->GetStaticShaderCache().GetColorMatrixFragmentShader());

	// TODO: Hmm. Push constants would be useful here.
	u8* uniform_buffer = draw.AllocatePSUniforms(sizeof(float) * 28);
	memcpy(uniform_buffer, colmat, (is_depth_copy ? sizeof(float) * 20 : sizeof(float) * 28));
	draw.CommitPSUniforms(sizeof(float) * 28);

	draw.SetPSSampler(0,
					  is_depth_copy ?
							framebuffer_mgr->GetEFBDepthTexture()->GetView() :
							framebuffer_mgr->GetEFBColorTexture()->GetView(),
					  scale_by_half ?
							object_cache->GetLinearSampler() :
							object_cache->GetPointSampler());

	VkRect2D dest_region = { { 0, 0 }, { m_texture->GetWidth(), m_texture->GetHeight() } };

	draw.BeginRenderPass(m_framebuffer, dest_region);

	draw.DrawQuad(scaled_src_rect.left,
				  scaled_src_rect.top,
				  scaled_src_rect.GetWidth(),
				  scaled_src_rect.GetHeight(),
				  framebuffer_mgr->GetEFBWidth(),
				  framebuffer_mgr->GetEFBHeight(),
				  0,
				  0,
				  config.width,
				  config.height);

	draw.EndRenderPass();

	// We touched everything, so put it back.
	m_parent->m_state_tracker->SetPendingRebind();

	// Transition back to shader resource.
	m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(const TCacheEntryBase* source, const MathUtil::Rectangle<int>& src_rect, const MathUtil::Rectangle<int>& dst_rect)
{
	const TCacheEntry* source_vk = static_cast<const TCacheEntry*>(source);
	VkCommandBuffer command_buffer = m_parent->m_command_buffer_mgr->GetCurrentCommandBuffer();

	// Fast path when not scaling the image.
	if (src_rect.GetWidth() == dst_rect.GetWidth() && src_rect.GetHeight() == dst_rect.GetHeight())
	{
		// These assertions should hold true unless the base code is passing us sizes too large, in
		// which case it should be fixed instead.
		_assert_msg_(VIDEO,
					 static_cast<u32>(src_rect.GetWidth()) <= source->config.width &&
					 static_cast<u32>(src_rect.GetHeight()) <= source->config.height,
					 "Source rect is too large for CopyRectangleFromTexture");

		_assert_msg_(VIDEO,
					 static_cast<u32>(dst_rect.GetWidth()) <= config.width &&
					 static_cast<u32>(dst_rect.GetHeight()) <= config.height,
					 "Dest rect is too large for CopyRectangleFromTexture");

		VkImageCopy image_copy =
		{
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, source->config.layers },										// VkImageSubresourceLayers    srcSubresource
			{ src_rect.left, src_rect.top, 0 },																// VkOffset3D                  srcOffset
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, config.layers },												// VkImageSubresourceLayers    dstSubresource
			{ dst_rect.left, dst_rect.top, 0 },																// VkOffset3D                  dstOffset
			{ static_cast<uint32_t>(src_rect.GetWidth()), static_cast<uint32_t>(src_rect.GetHeight()) }		// VkExtent3D                  extent
		};

		// Must be called outside of a render pass.
		m_parent->m_state_tracker->EndRenderPass();

		source_vk->m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		vkCmdCopyImage(command_buffer,
					   source_vk->m_texture->GetImage(),
					   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   m_texture->GetImage(),
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1,
					   &image_copy);

		m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		source_vk->m_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		return;
	}

	ERROR_LOG(VIDEO, "TextureCache::TCacheEntry::CopyRectangleFromTexture not implemented");
}

void TextureCache::TCacheEntry::Bind(unsigned int stage)
{
	m_parent->m_state_tracker->SetPSTexture(stage, m_texture->GetView());
}

bool TextureCache::TCacheEntry::Save(const std::string& filename, unsigned int level)
{
	ERROR_LOG(VIDEO, "TextureCache::TCacheEntry::Save not implemented");
	return false;
}

}		// namespace Vulkan
