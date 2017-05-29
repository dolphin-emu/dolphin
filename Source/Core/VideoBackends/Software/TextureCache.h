#pragma once

#include <memory>

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/SWTexture.h"
#include "VideoCommon/TextureCacheBase.h"

namespace SW
{

class TextureCache : public TextureCacheBase
{
public:
  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}
  void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, const void* palette,
    TLUTFormat format) override
  {
  }
  void CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
               u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half) override
  {
    EfbCopy::CopyEfb();
  }

private:
  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override
  {
    return std::make_unique<SWTexture>(config);
  }
  
  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, unsigned int cbuf_id, const float* colmat) override
  {
    EfbCopy::CopyEfb();
  }
};

} // namespace SW
