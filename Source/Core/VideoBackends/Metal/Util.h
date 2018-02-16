// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/TextureConfig.h"

namespace Metal
{
class CommandBufferManager;
class StateTracker;

namespace Util
{
size_t AlignBufferOffset(size_t offset, size_t alignment);

mtlpp::PixelFormat GetMetalPixelFormat(AbstractTextureFormat format);
AbstractTextureFormat GetAbstractTextureFormat(mtlpp::PixelFormat format);
u32 GetTexelSize(mtlpp::PixelFormat format);
u32 GetBlockSize(mtlpp::PixelFormat format);

// Clamps a VkRect2D to the specified dimensions.
// VkRect2D ClampRect2D(const VkRect2D& rect, u32 width, u32 height);

// Map {SRC,DST}_COLOR to {SRC,DST}_ALPHA
mtlpp::BlendFactor GetAlphaBlendFactor(mtlpp::BlendFactor factor);

std::array<VideoCommon::UtilityVertex, 4> GetQuadVertices(float z = 0.0f, u32 color = 0xffffffffu,
                                                          float u0 = 0.0f, float u1 = 1.0f,
                                                          float v0 = 0.0f, float v1 = 1.0f,
                                                          float uv_layer = 0.0f);
std::array<VideoCommon::UtilityVertex, 4> GetQuadVertices(const MathUtil::Rectangle<int>& uv_rect,
                                                          u32 tex_width, u32 tex_height,
                                                          float z = 0.0f, u32 color = 0);

}  // namespace Util
}  // namespace Metal
