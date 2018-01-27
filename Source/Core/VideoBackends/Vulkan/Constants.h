// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"

namespace Vulkan
{
// Number of command buffers. Having two allows one buffer to be
// executed whilst another is being built.

#if defined(_MSC_VER) && _MSC_VER <= 1800
enum
{
  NUM_COMMAND_BUFFERS = 2
};
#else
constexpr size_t NUM_COMMAND_BUFFERS = 2;
#endif

// Staging buffer usage - optimize for uploads or readbacks
enum STAGING_BUFFER_TYPE
{
  STAGING_BUFFER_TYPE_UPLOAD,
  STAGING_BUFFER_TYPE_READBACK
};

// Descriptor set layouts
enum DESCRIPTOR_SET_LAYOUT
{
  DESCRIPTOR_SET_LAYOUT_UNIFORM_BUFFERS,
  DESCRIPTOR_SET_LAYOUT_PIXEL_SHADER_SAMPLERS,
  DESCRIPTOR_SET_LAYOUT_SHADER_STORAGE_BUFFERS,
  DESCRIPTOR_SET_LAYOUT_TEXEL_BUFFERS,
  DESCRIPTOR_SET_LAYOUT_COMPUTE,
  NUM_DESCRIPTOR_SET_LAYOUTS
};

// Descriptor set bind points
enum DESCRIPTOR_SET_BIND_POINT
{
  DESCRIPTOR_SET_BIND_POINT_UNIFORM_BUFFERS,
  DESCRIPTOR_SET_BIND_POINT_PIXEL_SHADER_SAMPLERS,
  DESCRIPTOR_SET_BIND_POINT_STORAGE_OR_TEXEL_BUFFER,
  NUM_DESCRIPTOR_SET_BIND_POINTS
};

// We use four pipeline layouts:
//   - Standard
//       - Per-stage UBO (VS/GS/PS, VS constants accessible from PS)
//       - 8 combined image samplers (accessible from PS)
//   - BBox Enabled
//       - Same as standard, plus a single SSBO accessible from PS
//   - Push Constant
//       - Same as standard, plus 128 bytes of push constants, accessible from all stages.
//   - Texture Decoding
//       - Same as push constant, plus a single texel buffer accessible from PS.
//   - Compute
//       - 1 uniform buffer [set=0, binding=0]
//       - 4 combined image samplers [set=0, binding=1-4]
//       - 1 texel buffer [set=0, binding=5]
//       - 1 storage image [set=0, binding=6]
//       - 128 bytes of push constants
//
// All four pipeline layout share the first two descriptor sets (uniform buffers, PS samplers).
// The third descriptor set (see bind points above) is used for storage or texel buffers.
//
enum PIPELINE_LAYOUT
{
  PIPELINE_LAYOUT_STANDARD,
  PIPELINE_LAYOUT_BBOX,
  PIPELINE_LAYOUT_PUSH_CONSTANT,
  PIPELINE_LAYOUT_TEXTURE_CONVERSION,
  PIPELINE_LAYOUT_COMPUTE,
  NUM_PIPELINE_LAYOUTS
};

// Uniform buffer bindings within the first descriptor set
enum UNIFORM_BUFFER_DESCRIPTOR_SET_BINDING
{
  UBO_DESCRIPTOR_SET_BINDING_PS,
  UBO_DESCRIPTOR_SET_BINDING_VS,
  UBO_DESCRIPTOR_SET_BINDING_GS,
  NUM_UBO_DESCRIPTOR_SET_BINDINGS
};

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define CONSTEXPR(datatype, name, value)                                                           \
  enum name##_enum : datatype { name = value }
#else
#define CONSTEXPR(datatype, name, value) constexpr datatype name = value
#endif

// Maximum number of attributes per vertex (we don't have any more than this?)
CONSTEXPR(size_t, MAX_VERTEX_ATTRIBUTES, 16);

// Number of pixel shader texture slots
CONSTEXPR(size_t, NUM_PIXEL_SHADER_SAMPLERS, 8);

// Total number of binding points in the pipeline layout
CONSTEXPR(size_t, TOTAL_PIPELINE_BINDING_POINTS,
    NUM_UBO_DESCRIPTOR_SET_BINDINGS + NUM_PIXEL_SHADER_SAMPLERS + 1);

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define EFB_COLOR_TEXTURE_FORMAT ((VkFormat)VK_FORMAT_R8G8B8A8_UNORM)
#define EFB_DEPTH_TEXTURE_FORMAT ((VkFormat)VK_FORMAT_D32_SFLOAT)
#define EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT ((VkFormat)VK_FORMAT_R32_SFLOAT)
#define TEXTURECACHE_TEXTURE_FORMAT ((VkFormat)VK_FORMAT_R8G8B8A8_UNORM)
#else
// Format of EFB textures
CONSTEXPR(VkFormat, EFB_COLOR_TEXTURE_FORMAT, VK_FORMAT_R8G8B8A8_UNORM);
CONSTEXPR(VkFormat, EFB_DEPTH_TEXTURE_FORMAT, VK_FORMAT_D32_SFLOAT);
CONSTEXPR(VkFormat, EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT, VK_FORMAT_R32_SFLOAT);

// Format of texturecache textures
CONSTEXPR(VkFormat, TEXTURECACHE_TEXTURE_FORMAT, VK_FORMAT_R8G8B8A8_UNORM);
#endif

// Textures that don't fit into this buffer will be uploaded with a separate buffer (see below).
CONSTEXPR(size_t, INITIAL_TEXTURE_UPLOAD_BUFFER_SIZE, 16 * 1024 * 1024);
CONSTEXPR(size_t, MAXIMUM_TEXTURE_UPLOAD_BUFFER_SIZE, 64 * 1024 * 1024);

// Textures greater than 1024*1024 will be put in staging textures that are released after
// execution instead. A 2048x2048 texture is 16MB, and we'd only fit four of these in our
// streaming buffer and be blocking frequently. Games are unlikely to have textures this
// large anyway, so it's only really an issue for HD texture packs, and memory is not
// a limiting factor in these scenarios anyway.
CONSTEXPR(size_t, STAGING_TEXTURE_UPLOAD_THRESHOLD, 1024 * 1024 * 8);

// Streaming uniform buffer size
CONSTEXPR(size_t, INITIAL_UNIFORM_STREAM_BUFFER_SIZE, 16 * 1024 * 1024);
CONSTEXPR(size_t, MAXIMUM_UNIFORM_STREAM_BUFFER_SIZE, 32 * 1024 * 1024);

// Texel buffer size for palette and texture decoding.
CONSTEXPR(size_t, TEXTURE_CONVERSION_TEXEL_BUFFER_SIZE, 8 * 1024 * 1024);

// Push constant buffer size for utility shaders
CONSTEXPR(u32, PUSH_CONSTANT_BUFFER_SIZE, 128);

// Minimum number of draw calls per command buffer when attempting to preempt a readback operation.
CONSTEXPR(u32, MINIMUM_DRAW_CALLS_PER_COMMAND_BUFFER_FOR_READBACK, 10);

// Multisampling state info that we don't expose in VideoCommon.
union MultisamplingState
{
  BitField<0, 5, u32> samples;             // 1-16
  BitField<0, 1, u32> per_sample_shading;  // SSAA
  u32 hex;
};

}  // namespace Vulkan
