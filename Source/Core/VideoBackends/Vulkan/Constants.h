// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"

namespace Vulkan
{
// Number of command buffers.
constexpr size_t NUM_COMMAND_BUFFERS = 16;

// Number of frames in flight, will be used to decide how many descriptor pools are used
constexpr size_t NUM_FRAMES_IN_FLIGHT = 2;

// Staging buffer usage - optimize for uploads or readbacks
enum STAGING_BUFFER_TYPE
{
  STAGING_BUFFER_TYPE_UPLOAD,
  STAGING_BUFFER_TYPE_READBACK
};

// Descriptor set layouts
enum DESCRIPTOR_SET_LAYOUT
{
  DESCRIPTOR_SET_LAYOUT_STANDARD_UNIFORM_BUFFERS,
  DESCRIPTOR_SET_LAYOUT_STANDARD_SAMPLERS,
  DESCRIPTOR_SET_LAYOUT_STANDARD_SHADER_STORAGE_BUFFERS,
  DESCRIPTOR_SET_LAYOUT_UTILITY_UNIFORM_BUFFER,
  DESCRIPTOR_SET_LAYOUT_UTILITY_SAMPLERS,
  DESCRIPTOR_SET_LAYOUT_COMPUTE,
  NUM_DESCRIPTOR_SET_LAYOUTS
};

// We use four pipeline layouts:
//   - Standard
//       - Per-stage UBO (VS/GS/PS, VS constants accessible from PS) [set=0, binding=0-3]
//       - 8 combined image samplers (accessible from PS) [set=1, binding=0-7]
//       - 1 SSBO accessible from PS if supported [set=2, binding=0]
//   - Uber
//       - Like standard, plus 1 SSBO accessible from VS if supported [set=2, binding=1]
//   - Utility
//       - 1 combined UBO, accessible from VS/GS/PS [set=0, binding=0]
//       - 8 combined image samplers (accessible from PS) [set=1, binding=0-7]
//       - 1 texel buffer (accessible from PS) [set=1, binding=8]
//   - Compute
//       - 1 uniform buffer [set=0, binding=0]
//       - 8 combined image samplers [set=0, binding=1-8]
//       - 2 texel buffers [set=0, binding=9-10]
//       - 8 storage image [set=0, binding=11-18]
//
// All four pipeline layout share the first two descriptor sets (uniform buffers, PS samplers).
// The third descriptor set (see bind points above) is used for storage or texel buffers.
//
enum PIPELINE_LAYOUT
{
  PIPELINE_LAYOUT_STANDARD,
  PIPELINE_LAYOUT_UBER,
  PIPELINE_LAYOUT_UTILITY,
  PIPELINE_LAYOUT_COMPUTE,
  NUM_PIPELINE_LAYOUTS
};

// Uniform buffer bindings within the first descriptor set
enum UNIFORM_BUFFER_DESCRIPTOR_SET_BINDING
{
  UBO_DESCRIPTOR_SET_BINDING_PS,
  UBO_DESCRIPTOR_SET_BINDING_VS,
  UBO_DESCRIPTOR_SET_BINDING_PS_CUST,
  UBO_DESCRIPTOR_SET_BINDING_GS,
  NUM_UBO_DESCRIPTOR_SET_BINDINGS
};

// Maximum number of attributes per vertex (we don't have any more than this?)
constexpr u32 MAX_VERTEX_ATTRIBUTES = 16;

// Number of pixel shader texture slots
constexpr u32 NUM_UTILITY_PIXEL_SAMPLERS = 8;

// Number of texel buffer binding points.
constexpr u32 NUM_COMPUTE_TEXEL_BUFFERS = 2;

// Textures that don't fit into this buffer will be uploaded with a separate buffer (see below).
constexpr u32 TEXTURE_UPLOAD_BUFFER_SIZE = 32 * 1024 * 1024;

// Textures greater than 1024*1024 will be put in staging textures that are released after
// execution instead. A 2048x2048 texture is 16MB, and we'd only fit four of these in our
// streaming buffer and be blocking frequently. Games are unlikely to have textures this
// large anyway, so it's only really an issue for HD texture packs, and memory is not
// a limiting factor in these scenarios anyway.
constexpr u32 STAGING_TEXTURE_UPLOAD_THRESHOLD = 1024 * 1024 * 4;
}  // namespace Vulkan
