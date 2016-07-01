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
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureEncoder.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureDecoder.h"

namespace Vulkan {

TextureEncoder::TextureEncoder(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
	, m_state_tracker(state_tracker)
{
  if (!CompileShaders())
    PanicAlert("Failed to compile shaders");

	if (!CreateEncodingRenderPass())
		PanicAlert("Failed to create encode render pass");

	if (!CreateEncodingTexture())
		PanicAlert("Failed to create encoding texture");

	if (!CreateEncodingDownloadBuffer())
		PanicAlert("Failed to create encoding download buffer");
}

TextureEncoder::~TextureEncoder()
{
	if (m_encoding_render_pass != VK_NULL_HANDLE)
		m_command_buffer_mgr->DeferResourceDestruction(m_encoding_render_pass);

	if (m_encoding_texture_framebuffer != VK_NULL_HANDLE)
		m_command_buffer_mgr->DeferResourceDestruction(m_encoding_texture_framebuffer);

	m_encoding_texture.reset();

	if (m_texture_download_buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(m_object_cache->GetDevice(), m_texture_download_buffer, nullptr);
		vkFreeMemory(m_object_cache->GetDevice(), m_texture_download_buffer_memory, nullptr);
	}
}

void TextureEncoder::EncodeTextureToRam(VkImageView src_texture, u8* dest_ptr, u32 format, u32 native_width, u32 bytes_per_row,
										u32 num_blocks_y, u32 memory_stride, PEControl::PixelFormat src_format,
										bool is_intensity, int scale_by_half, const EFBRectangle& src_rect)
{
	if (m_texture_encoding_shaders[format] == VK_NULL_HANDLE)
	{
		ERROR_LOG(VIDEO, "Missing encoding fragment shader for format %u", format);
		return;
	}

	UtilityShaderDraw draw(m_object_cache, m_command_buffer_mgr, m_encoding_render_pass,
                         m_object_cache->GetStaticShaderCache().GetScreenQuadVertexShader(),
                         VK_NULL_HANDLE,
                         m_texture_encoding_shaders[format]);

	// Allocate uniform buffer - int4 of left,top,native_width,scale
	// TODO: Replace with push constants
  s32 position_uniform[4] = { src_rect.left, src_rect.top, static_cast<s32>(native_width), scale_by_half ? 2 : 1 };
	u8* uniform_buffer_ptr = draw.AllocatePSUniforms(sizeof(position_uniform));
	memcpy(uniform_buffer_ptr, position_uniform, sizeof(position_uniform));
	draw.CommitPSUniforms(sizeof(position_uniform));

	// Doesn't make sense to linear filter depth values
	draw.SetPSSampler(0, src_texture, (scale_by_half && src_format != PEControl::Z24) ? m_object_cache->GetLinearSampler() : m_object_cache->GetPointSampler());

	// Before drawing - make sure we're not in a render pass
	m_state_tracker->EndRenderPass();

	// Ensure the source image is in COLOR_ATTACHMENT state. We don't care about the old contents of it, so forcing to UNDEFINED is okay here
	m_encoding_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	m_encoding_texture->TransitionToLayout(m_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	u32 render_width = bytes_per_row / sizeof(u32);
	u32 render_height = num_blocks_y;
	Util::SetViewportAndScissor(m_command_buffer_mgr->GetCurrentCommandBuffer(), 0, 0, render_width, render_height);

	// TODO: We could use compute shaders here.
	VkRect2D render_region = { { 0, 0 }, { render_width, render_height } };
	draw.BeginRenderPass(m_encoding_texture_framebuffer, render_region);
	draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
	draw.EndRenderPass();

	// Transition both the image and the buffer before copying
	m_encoding_texture->TransitionToLayout(m_command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	Util::BufferMemoryBarrier(m_command_buffer_mgr->GetCurrentCommandBuffer(), m_texture_download_buffer,
							  0, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
							  0, VK_WHOLE_SIZE, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

	// Copy from the encoding texture to the download buffer
  u32 aligned_width = static_cast<u32>(Util::AlignValue(render_width, m_object_cache->GetTextureUploadPitchAlignment()));
	VkBufferImageCopy image_copy =
	{
		0,														// VkDeviceSize                bufferOffset
		aligned_width,												// uint32_t                    bufferRowLength
		render_height,											// uint32_t                    bufferImageHeight
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },					// VkImageSubresourceLayers    imageSubresource
		{ 0, 0, 0 },											// VkOffset3D                  imageOffset
		{ render_width, render_height, 1 }						// VkExtent3D                  imageExtent
	};
	vkCmdCopyImageToBuffer(m_command_buffer_mgr->GetCurrentCommandBuffer(),
						   m_encoding_texture->GetImage(),
						   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   m_texture_download_buffer,
						   1, &image_copy);

	// Transition buffer to host read
	Util::BufferMemoryBarrier(m_command_buffer_mgr->GetCurrentCommandBuffer(), m_texture_download_buffer,
							  VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
							  VK_ACCESS_HOST_READ_BIT,
							  0, VK_WHOLE_SIZE, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

	// Submit command buffer and wait until completion.
	// Also tell the state tracker to use new descriptor sets, since we're moving to a new cmdbuffer
	m_command_buffer_mgr->ExecuteCommandBuffer(true);
	m_state_tracker->InvalidateDescriptorSets();
	m_state_tracker->SetPendingRebind();

	// Map buffer and copy to destination
	// TODO: With vulkan can we leave this mapped?
	u32 source_stride = aligned_width * sizeof(u32);
	void* source_ptr;
	VkResult res = vkMapMemory(m_object_cache->GetDevice(), m_texture_download_buffer_memory, 0, source_stride * render_height, 0, &source_ptr);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkMapMemory failed: ");
		return;
	}

	if (source_stride == memory_stride)
	{
		memcpy(dest_ptr, source_ptr, source_stride * render_height);
	}
	else
	{
		u32 copy_size = std::min(source_stride, bytes_per_row);
		u8* current_source_ptr = reinterpret_cast<u8*>(source_ptr);
		u8* current_dest_ptr = dest_ptr;
		for (u32 row = 0; row < render_height; row++)
		{
			memcpy(current_dest_ptr, current_source_ptr, copy_size);
			current_source_ptr += source_stride;
			current_dest_ptr += memory_stride;
		}
	}

	vkUnmapMemory(m_object_cache->GetDevice(), m_texture_download_buffer_memory);
}

bool TextureEncoder::CompileShaders()
{
  // Texture encoding shaders
  // TODO: A better way of generating these
  static const u32 texture_encoding_shader_formats[] =
  {
    GX_TF_I4, GX_TF_I8, GX_TF_IA4, GX_TF_IA8, GX_TF_RGB565, GX_TF_RGB5A3, GX_TF_RGBA8,
    GX_CTF_R4, GX_CTF_RA4, GX_CTF_RA8, GX_CTF_A8, GX_CTF_R8, GX_CTF_G8, GX_CTF_B8,
    GX_CTF_RG8, GX_CTF_GB8, GX_CTF_Z8H, GX_TF_Z8, GX_CTF_Z16R, GX_TF_Z16, GX_TF_Z24X8,
    GX_CTF_Z4, GX_CTF_Z8M, GX_CTF_Z8L, GX_CTF_Z16L
  };
  for (size_t i = 0; i < ARRAYSIZE(texture_encoding_shader_formats); i++)
  {
    u32 format = texture_encoding_shader_formats[i];
    const char* shader_source = TextureConversionShader::GenerateEncodingShader(format, API_VULKAN);
    m_texture_encoding_shaders[format] = m_object_cache->GetPixelShaderCache().CompileAndCreateShader(shader_source);
    if (m_texture_encoding_shaders[format] == VK_NULL_HANDLE)
      return false;
  }

  return true;
}

bool TextureEncoder::CreateEncodingRenderPass()
{
	VkAttachmentDescription attachments[] = {
		{
			0,
			VK_FORMAT_R8G8B8A8_UNORM,
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

	VkResult res = vkCreateRenderPass(m_object_cache->GetDevice(), &pass_info, nullptr, &m_encoding_render_pass);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateRenderPass (Encode) failed: ");
		return false;
	}

	return true;
}

bool TextureEncoder::CreateEncodingTexture()
{
	// From OGL: Why do we create a 1024 height texture?
	m_encoding_texture = Texture2D::Create(m_object_cache, m_command_buffer_mgr,
										   ENCODING_TEXTURE_WIDTH, ENCODING_TEXTURE_HEIGHT, 1, 1,
										   VK_FORMAT_R8G8B8A8_UNORM,
										   VK_IMAGE_VIEW_TYPE_2D,
										   VK_IMAGE_TILING_OPTIMAL,
										   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	if (!m_encoding_texture)
		return false;

	VkImageView framebuffer_attachments[] = { m_encoding_texture->GetView() };
	VkFramebufferCreateInfo framebuffer_info =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		m_encoding_render_pass,
		ARRAYSIZE(framebuffer_attachments),
		framebuffer_attachments,
		m_encoding_texture->GetWidth(),
		m_encoding_texture->GetHeight(),
		m_encoding_texture->GetLayers()
	};

	VkResult res = vkCreateFramebuffer(m_object_cache->GetDevice(), &framebuffer_info, nullptr, &m_encoding_texture_framebuffer);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
		return nullptr;
	}

	return true;
}

bool TextureEncoder::CreateEncodingDownloadBuffer()
{
	VkDeviceSize buffer_texture_width = Util::AlignValue(ENCODING_TEXTURE_WIDTH, m_object_cache->GetTextureUploadPitchAlignment());
	VkDeviceSize buffer_texture_height = ENCODING_TEXTURE_HEIGHT;
	VkDeviceSize buffer_size = buffer_texture_width * buffer_texture_height * sizeof(u32);
  m_texture_download_buffer_size = buffer_size;

	VkBufferCreateInfo buffer_create_info =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,			// VkStructureType        sType
		nullptr,										// const void*            pNext
		0,												// VkBufferCreateFlags    flags
		buffer_size,									// VkDeviceSize           size
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,				// VkBufferUsageFlags     usage
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode          sharingMode
		0,												// uint32_t               queueFamilyIndexCount
		nullptr											// const uint32_t*        pQueueFamilyIndices
	};
	VkResult res = vkCreateBuffer(m_object_cache->GetDevice(),
								  &buffer_create_info,
								  nullptr,
								  &m_texture_download_buffer);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
		return false;
	}

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(m_object_cache->GetDevice(), m_texture_download_buffer, &memory_requirements);

	uint32_t memory_type_index = m_object_cache->GetMemoryType(memory_requirements.memoryTypeBits,
															   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	VkMemoryAllocateInfo memory_allocate_info =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,			// VkStructureType    sType
		nullptr,										// const void*        pNext
		memory_requirements.size,						// VkDeviceSize       allocationSize
		memory_type_index								// uint32_t           memoryTypeIndex
	};
	res = vkAllocateMemory(m_object_cache->GetDevice(), &memory_allocate_info, nullptr, &m_texture_download_buffer_memory);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
		vkDestroyBuffer(m_object_cache->GetDevice(), m_texture_download_buffer, nullptr);
		return false;
	}

  res = vkBindBufferMemory(m_object_cache->GetDevice(), m_texture_download_buffer, m_texture_download_buffer_memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindBufferMemory failed: ");
    return false;
  }

	return true;
}

}		// namespace Vulkan
