// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
// Number of command buffers. Having two allows one buffer to be
// executed whilst another is being built.
constexpr size_t NUM_COMMAND_BUFFERS = 2;

// Descriptor sets
enum DESCRIPTOR_SET
{
  DESCRIPTOR_SET_UNIFORM_BUFFERS,
  DESCRIPTOR_SET_PIXEL_SHADER_SAMPLERS,
  DESCRIPTOR_SET_SHADER_STORAGE_BUFFERS,
  NUM_DESCRIPTOR_SETS
};

// Uniform buffer bindings within the first descriptor set
enum UNIFORM_BUFFER_DESCRIPTOR_SET_BINDING
{
  UBO_DESCRIPTOR_SET_BINDING_VS,
  UBO_DESCRIPTOR_SET_BINDING_GS,
  UBO_DESCRIPTOR_SET_BINDING_PS,
  NUM_UBO_DESCRIPTOR_SET_BINDINGS
};

// Maximum number of attributes per vertex (we don't have any more than this?)
constexpr size_t MAX_VERTEX_ATTRIBUTES = 16;

// Number of pixel shader texture slots
constexpr size_t NUM_PIXEL_SHADER_SAMPLERS = 8;

// Total number of binding points in the pipeline layout
constexpr size_t TOTAL_PIPELINE_BINDING_POINTS =
    NUM_UBO_DESCRIPTOR_SET_BINDINGS + NUM_PIXEL_SHADER_SAMPLERS + 1;

// Format of EFB textures
constexpr VkFormat EFB_COLOR_TEXTURE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat EFB_DEPTH_TEXTURE_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT = VK_FORMAT_R32_SFLOAT;

// Format of texturecache textures
constexpr VkFormat TEXTURECACHE_TEXTURE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

// Textures that don't fit into this buffer will be uploaded with a separate buffer.
constexpr size_t INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE = 16 * 1024 * 1024;
constexpr size_t MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE = 64 * 1024 * 1024;

// Streaming uniform buffer size
constexpr size_t INITIAL_UNIFORM_STREAM_BUFFER_SIZE = 16 * 1024 * 1024;
constexpr size_t MAXIMUM_UNIFORM_STREAM_BUFFER_SIZE = 32 * 1024 * 1024;

// Do we not have this anywhere else?
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) ((sizeof(a) / sizeof(*(a))) / static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#endif

// Rasterization state info
union RasterizationState {
  BitField<0, 2, VkCullModeFlags> cull_mode;

  u32 hex;
};

// Depth state info
union DepthStencilState {
  BitField<0, 1, u32> test_enable;
  BitField<1, 1, u32> write_enable;
  BitField<2, 3, VkCompareOp> compare_op;

  u32 hex;
};

// Blend state info
union BlendState {
  BitField<0, 1, u32> blend_enable;
  BitField<1, 3, VkBlendOp> blend_op;
  BitField<4, 4, VkColorComponentFlags> write_mask;
  BitField<8, 5, VkBlendFactor> src_blend;
  BitField<13, 5, VkBlendFactor> dst_blend;
  BitField<18, 1, u32> use_dst_alpha;
  BitField<19, 1, u32> logic_op_enable;
  BitField<20, 4, VkLogicOp> logic_op;

  u32 hex;
};

// Sampler info
union SamplerState {
  BitField<0, 1, VkFilter> min_filter;
  BitField<1, 1, VkFilter> mag_filter;
  BitField<2, 1, VkSamplerMipmapMode> mipmap_mode;
  BitField<3, 2, VkSamplerAddressMode> wrap_u;
  BitField<5, 2, VkSamplerAddressMode> wrap_v;
  BitField<7, 8, u32> min_lod;
  BitField<15, 8, u32> max_lod;
  BitField<23, 6, s32> lod_bias;    // tm0.lod_bias (8 bits) / 32 gives us 0-7.
  BitField<29, 3, u32> anisotropy;  // max_anisotropy = 1 << anisotropy, max of 16, so range 0-4.

  u32 hex;
};

}  // namespace Vulkan
