// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/TextureCacheBase.h"

namespace DX11
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache();
  ~TextureCache();

private:
  std::unique_ptr<AbstractTextureBase>
  CreateTexture(const AbstractTextureBase::TextureConfig& config) override;

  u64 EncodeToRamFromTexture(u32 address, void* source_texture, u32 SourceW, u32 SourceH,
                             bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf,
                             const EFBRectangle& source)
  {
    return 0;
  };

  void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, void* palette,
                      TlutFormat format) override;

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
               bool isIntensity, bool scaleByHalf) override;

  void CompileShaders() override {}
  void DeleteShaders() override {}
  ID3D11Buffer* palette_buf;
  ID3D11ShaderResourceView* palette_buf_srv;
  ID3D11Buffer* palette_uniform;
  ID3D11PixelShader* palette_pixel_shader[3];
};
}
