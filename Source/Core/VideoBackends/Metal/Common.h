// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractTexture.h"
#include "mtlpp.hpp"

namespace Metal
{
constexpr u32 UNIFORM_BUFFER_ALIGNMENT = 256;
constexpr u32 TEXTURE_STREAMING_BUFFER_ALIGNMENT = 1024;
constexpr u32 MAX_TEXTURE_BINDINGS = 8;
constexpr u32 FIRST_VERTEX_UBO_INDEX = 1;
constexpr u32 FIRST_PIXEL_UBO_INDEX = 0;
constexpr u32 NUM_PIXEL_SHADER_UBOS = 2;

constexpr AbstractTextureFormat EFB_COLOR_TEXTURE_FORMAT = AbstractTextureFormat::RGBA8;
constexpr AbstractTextureFormat EFB_DEPTH_TEXTURE_FORMAT = AbstractTextureFormat::D32F;

// TODO: Move these to common.
constexpr u32 INITIAL_UNIFORM_BUFFER_SIZE = 16 * 1024 * 1024;
constexpr u32 MAX_UNIFORM_BUFFER_SIZE = 32 * 1024 * 1024;
constexpr u32 INITIAL_TEXTURE_BUFFER_SIZE = 16 * 1024 * 1024;
constexpr u32 MAX_TEXTURE_BUFFER_SIZE = 32 * 1024 * 1024;
constexpr u32 UTILITY_VERTEX_STREAMING_BUFFER_SIZE = 1 * 1024 * 1024;

using CommandBufferId = u64;
}  // namespace Metal
