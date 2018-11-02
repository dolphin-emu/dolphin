// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"

#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

struct EFBCopyParams;
class AbstractStagingTexture;

namespace OGL
{
// Converts textures between formats using shaders
// TODO: support multiple texture formats
namespace TextureConverter
{
void Init();
void Shutdown();

// returns size of the encoded data (in bytes)
void EncodeToRamFromTexture(
    AbstractStagingTexture* dest, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
    u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect, bool scale_by_half,
    float y_scale, float gamma, float clamp_top, float clamp_bottom,
    const TextureCacheBase::CopyFilterCoefficientArray& filter_coefficients);
}

}  // namespace OGL
