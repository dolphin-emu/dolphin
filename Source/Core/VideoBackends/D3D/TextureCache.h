// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoCommon/TextureCacheBase.h"

class AbstractTexture;
struct TextureConfig;

namespace DX11
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache();
  ~TextureCache();

private:
  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;

  u64 EncodeToRamFromTexture(u32 address, void* source_texture, u32 SourceW, u32 SourceH,
                             bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf,
                             const EFBRectangle& source)
  {
    return 0;
  };

  void ConvertTexture(TCacheEntry* destination, TCacheEntry* source, const void* palette,
                      TLUTFormat format) override;

  void CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
               u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half) override;

  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, unsigned int cbuf_id, const float* colmat) override;

  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}
  ID3D11Buffer* palette_buf;
  ID3D11ShaderResourceView* palette_buf_srv;
  ID3D11Buffer* palette_uniform;
  ID3D11PixelShader* palette_pixel_shader[3];
};
}
