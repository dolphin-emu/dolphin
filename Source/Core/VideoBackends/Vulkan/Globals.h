// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

// Number of command buffers. Having two allows one buffer to be
// executed whilst another is being built.
constexpr size_t NUM_COMMAND_BUFFERS = 2;

// Descriptor sets
// TODO: Split to per-stage or multi-set depending on update frequency.
enum DESCRIPTOR_SET
{
	//DESCRIPTOR_SET_VS,
	//DESCRIPTOR_SET_GS,
	//DESCRIPTOR_SET_FS,
	DESCRIPTOR_SET_COMBINED,
	NUM_DESCRIPTOR_SETS
};

enum COMBINED_DESCRIPTOR_SET_BINDING
{
	COMBINED_DESCRIPTOR_SET_BINDING_VS_UBO = 2,
	COMBINED_DESCRIPTOR_SET_BINDING_GS_UBO = 3,
	COMBINED_DESCRIPTOR_SET_BINDING_PS_UBO = 1,
	COMBINED_DESCRIPTOR_SET_BINDING_PS_SAMPLERS = 0,
	COMBINED_DESCRIPTOR_SET_BINDING_PS_SSBO = 4,
	NUM_COMBINED_DESCRIPTOR_SET_BINDINGS = 5
};

// Maximum number of attributes per vertex (we don't have any more than this?)
constexpr size_t MAX_VERTEX_ATTRIBUTES = 16;

// Number of pixel shader texture slots
constexpr size_t NUM_PIXEL_SHADER_SAMPLERS = 8;

// Format of EFB textures
constexpr VkFormat EFB_COLOR_TEXTURE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat EFB_DEPTH_TEXTURE_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT = VK_FORMAT_R32_SFLOAT;

// Format of texturecache textures
constexpr VkFormat TEXTURECACHE_TEXTURE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

// Textures that don't fit into this buffer will be uploaded with a separate buffer.
constexpr size_t INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE = 4 * 1024 * 1024;
constexpr size_t MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE = 64 * 1024 * 1024;

// Streaming uniform buffer size
constexpr size_t INITIAL_UNIFORM_STREAM_BUFFER_SIZE = 4 * 1024 * 1024;
constexpr size_t MAXIMUM_UNIFORM_STREAM_BUFFER_SIZE = 16 * 1024 * 1024;

// Rasterization state info
union RasterizationState
{
	BitField<0, 2, VkCullModeFlags> cull_mode;

	u32 hex;
};

// Depth state info
union DepthStencilState
{
	BitField<0, 1, u32> test_enable;
	BitField<1, 1, u32> write_enable;
	BitField<2, 3, VkCompareOp> compare_op;

	u32 hex;
};

// Blend state info
union BlendState
{
	BitField<0, 1, u32> blend_enable;
	BitField<1, 3, VkBlendOp> blend_op;
	BitField<4, 4, VkColorComponentFlags> write_mask;
	BitField<8, 5, VkBlendFactor> src_blend;
	BitField<13, 5, VkBlendFactor> dst_blend;
	BitField<18, 1, u32> use_dst_alpha;

	u32 hex;
};

// Sampler info
union SamplerState
{
	BitField<0, 1, VkFilter> min_filter;
	BitField<1, 1, VkFilter> mag_filter;
	BitField<2, 1, VkSamplerMipmapMode> mipmap_mode;
	BitField<3, 2, VkSamplerAddressMode> wrap_u;
	BitField<5, 2, VkSamplerAddressMode> wrap_v;
	BitField<7, 8, u32> min_lod;
	BitField<15, 8, u32> max_lod;
	BitField<23, 8, s32> lod_bias;	

	u32 hex;
};

}		// namespace Vulkan