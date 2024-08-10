#pragma once

#include "VideoBackends/Software/TextureEncoder.h"
#include "VideoCommon/TextureCacheBase.h"

namespace SW
{
class TextureCache : public TextureCacheBase
{
protected:
  void CopyEFB(AbstractStagingTexture* dst, const EFBCopyParams& params, const u32 native_width,
               const u32 bytes_per_row, const u32 num_blocks_y, const u32 memory_stride,
               const MathUtil::Rectangle<int>& src_rect, const bool scale_by_half, bool linear_filter,
               const float y_scale, const float gamma, bool clamp_top, bool clamp_bottom,
               const std::array<u32, 3>& filter_coefficients) override
  {
    TextureEncoder::Encode(dst, params, native_width, bytes_per_row, num_blocks_y, memory_stride,
                           src_rect, scale_by_half, y_scale, gamma);
  }
  void CopyEFBToCacheEntry(RcTcacheEntry& entry, bool is_depth_copy,
                           const MathUtil::Rectangle<int>& src_rect, bool scale_by_half,
                           bool linear_filter, EFBCopyFormat dst_format, bool is_intensity,
                           float gamma, bool clamp_top, bool clamp_bottom,
                           const std::array<u32, 3>& filter_coefficients) override
  {
    // TODO: If we ever want to "fake" vram textures, we would need to implement this
  }
};

}  // namespace SW
