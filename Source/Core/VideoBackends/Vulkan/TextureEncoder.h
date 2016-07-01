// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/StreamBuffer.h"

#include "VideoCommon/VideoCommon.h"

namespace Vulkan {

class CommandBufferManager;
class ObjectCache;
class StateTracker;
class Texture2D;

class TextureEncoder
{
public:
	TextureEncoder(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker);
	~TextureEncoder();

	void EncodeTextureToRam(VkImageView src_texture, u8* dest_ptr, u32 format, u32 native_width, u32 bytes_per_row,
							u32 num_blocks_y, u32 memory_stride, PEControl::PixelFormat src_format,
							bool is_intensity, int scale_by_half, const EFBRectangle& source);

private:
  // From OGL.
  static const u32 NUM_TEXTURE_ENCODING_SHADERS = 64;
  static const u32 ENCODING_TEXTURE_WIDTH = EFB_WIDTH * 4;
  static const u32 ENCODING_TEXTURE_HEIGHT = 1024;

  bool CompileShaders();
	bool CreateEncodingRenderPass();
	bool CreateEncodingTexture();
	bool CreateEncodingDownloadBuffer();

	ObjectCache* m_object_cache = nullptr;
	CommandBufferManager* m_command_buffer_mgr = nullptr;
	StateTracker* m_state_tracker = nullptr;

  std::array<VkShaderModule, NUM_TEXTURE_ENCODING_SHADERS> m_texture_encoding_shaders = {};

	VkRenderPass m_encoding_render_pass = VK_NULL_HANDLE;

	std::unique_ptr<Texture2D> m_encoding_texture;
	VkFramebuffer m_encoding_texture_framebuffer = VK_NULL_HANDLE;

	VkBuffer m_texture_download_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_texture_download_buffer_memory = nullptr;
	VkDeviceSize m_texture_download_buffer_size = 0;
};

} // namespace Vulkan
