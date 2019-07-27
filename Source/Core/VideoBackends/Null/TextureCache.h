// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/TextureCacheBase.h"

namespace Null
{
class TextureCache final : public TextureCacheBase
{
protected:
  void CopyEFB(AbstractStagingTexture* dst, const EFBCopyParams& params, u32 native_width,
               u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
               const MathUtil::Rectangle<int>& src_rect, bool scale_by_half, bool linear_filter,
               float y_scale, float gamma, bool clamp_top, bool clamp_bottom,
               const EFBCopyFilterCoefficients& filter_coefficients) override
  {
  }

  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                           const MathUtil::Rectangle<int>& src_rect, bool scale_by_half,
                           bool linear_filter, EFBCopyFormat dst_format, bool is_intensity,
                           float gamma, bool clamp_top, bool clamp_bottom,
                           const EFBCopyFilterCoefficients& filter_coefficients) override
  {
  }
};

}  // namespace Null
