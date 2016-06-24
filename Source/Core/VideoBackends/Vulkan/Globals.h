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

// Maximum number of attributes per vertex (we don't have any more than this?)
constexpr size_t MAX_VERTEX_ATTRIBUTES = 16;

// Number of pixel shader texture slots
constexpr size_t NUM_PIXEL_SHADER_SAMPLERS = 8;

// Format of EFB textures
constexpr VkFormat EFB_COLOR_TEXTURE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat EFB_DEPTH_TEXTURE_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT = VK_FORMAT_R32_SFLOAT;

// Rasterization state info
union RasterizationState
{
	BitField<0, 2, VkCullModeFlags> cull_mode;

	u32 hex;
};

// Depth state info
struct DepthStencilState
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
	BitField<18, 3, u32> dstalpha_mode;

	u32 hex;
};

}		// namespace Vulkan