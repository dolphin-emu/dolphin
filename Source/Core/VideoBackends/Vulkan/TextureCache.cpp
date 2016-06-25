// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

// Textures that don't fit into this buffer will be uploaded with a separate buffer.
constexpr size_t INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE = 4 * 1024 * 1024;
constexpr size_t MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE = 64 * 1024 * 1024;

TextureCache::TextureCache(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
	, m_state_tracker(state_tracker)
{
	m_texture_upload_buffer = StreamBuffer::Create(object_cache, command_buffer_mgr, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE, MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE);
	if (!m_texture_upload_buffer)
		PanicAlert("Failed to create texture upload buffer");
}

TextureCache::~TextureCache()
{

}

void TextureCache::CompileShaders()
{

}

void TextureCache::DeleteShaders()
{

}

void TextureCache::ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette, TlutFormat format)
{
	ERROR_LOG(VIDEO, "TextureCache::ConvertTexture not implemented");
}

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
	PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
	bool is_intensity, bool scale_by_half)
{
	ERROR_LOG(VIDEO, "TextureCache::CopyEFB not implemented");
}

TextureCacheBase::TCacheEntryBase* TextureCache::CreateTexture(const TCacheEntryConfig& config)
{
	// Allocate texture object
	std::unique_ptr<Texture2D> texture = Texture2D::Create(m_object_cache, m_command_buffer_mgr,
		config.width, config.height, config.levels, config.layers, TEXTURECACHE_TEXTURE_FORMAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	if (!texture)
		return nullptr;

	return new TCacheEntry(config, this, std::move(texture));
}

TextureCache::TCacheEntry::TCacheEntry(const TCacheEntryConfig& _config, TextureCache* parent, std::unique_ptr<Texture2D> texture)
	: TCacheEntryBase(_config)
	, m_parent(parent)
	, m_texture(std::move(texture))
{

}

TextureCache::TCacheEntry::~TCacheEntry()
{
	// Texture is automatically cleaned up.
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
	VkImageSubresource image_subresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0 };
	VkSubresourceLayout subresource_layout;
	vkGetImageSubresourceLayout(command_buffer_mgr->GetDevice(), m_texture->GetImage(), &image_subresource, &subresource_layout);

	// Allocate memory from the streaming buffer for the texture data.
	VkDeviceSize upload_pitch = Util::AlignValue(subresource_layout.rowPitch, object_cache->GetTextureUploadPitchAlignment());
	VkDeviceSize upload_size = upload_pitch * height;

	// TODO: What should the alignment be here?
	// TODO: Handle cases where the texture does not fit into the streaming buffer, we need to allocate a temporary buffer.
	if (!m_parent->m_texture_upload_buffer->ReserveMemory(upload_size, object_cache->GetTextureUploadAlignment(), false))
	{
		// Execute the command buffer first.
		Util::ExecuteCurrentCommandsAndRestoreState(command_buffer_mgr);

		// Try allocating again. This may cause a fence wait.
		if (!upload_buffer->ReserveMemory(upload_size, object_cache->GetTextureUploadAlignment(), false))
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

	// Copy from the streaming buffer to the actual image, tiling in the process.
	VkBufferImageCopy image_copy =
	{
		image_upload_buffer_offset,								// VkDeviceSize                bufferOffset
		static_cast<uint32_t>(subresource_layout.rowPitch),		// uint32_t                    bufferRowLength
		height,													// uint32_t                    bufferImageHeight
		{ VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 },				// VkImageSubresourceLayers    imageSubresource
		{ 0, 0, 0 },											// VkOffset3D                  imageOffset
		{ width, height, 1 }									// VkExtent3D                  imageExtent
	};

	// Transition the texture to a transfer destination, invoke the transfer, then transition back.
	// TODO: Only transition the layer we're copying to.
	m_texture->TransitionToLayout(command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdCopyBufferToImage(command_buffer_mgr->GetCurrentCommandBuffer(), image_upload_buffer, m_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);
	m_texture->TransitionToLayout(command_buffer_mgr->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCache::TCacheEntry::FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& src_rect, bool scale_by_half, unsigned int cbufid, const float* colmat)
{
	ERROR_LOG(VIDEO, "TextureCache::TCacheEntry::FromRenderTarget not implemented");
}

void TextureCache::TCacheEntry::CopyRectangleFromTexture(const TCacheEntryBase* source, const MathUtil::Rectangle<int>& srcrect, const MathUtil::Rectangle<int>& dstrect)
{
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
