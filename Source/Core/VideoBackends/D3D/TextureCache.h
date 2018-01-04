// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoCommon/TextureCacheBase.h"

namespace DX11
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache();
  ~TextureCache();

private:
  struct TCacheEntry : TCacheEntryBase
  {
    D3DTexture2D* const texture;

    TCacheEntry(const TCacheEntryConfig& config, D3DTexture2D* _tex)
        : TCacheEntryBase(config), texture(_tex)
    {
    }
    ~TCacheEntry();

    void CopyRectangleFromTexture(const TCacheEntryBase* source,
                                  const MathUtil::Rectangle<int>& srcrect,
                                  const MathUtil::Rectangle<int>& dstrect) override;

    void Load(const u8* buffer, u32 width, u32 height, u32 expanded_width, u32 levels) override;

    void FromRenderTarget(bool is_depth_copy, const EFBRectangle& srcRect, bool scaleByHalf,
                          unsigned int cbufid, const float* colmat) override;

    void Bind(unsigned int stage) override;
    bool Save(const std::string& filename, unsigned int level) override;
  };

  TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

  u64 EncodeToRamFromTexture(u32 address, void* source_texture, u32 SourceW, u32 SourceH,
                             bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf,
                             const EFBRectangle& source)
  {
    return 0;
  };

  void ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette,
                      TlutFormat format) override;

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, bool is_depth_copy, const EFBRectangle& srcRect, bool isIntensity,
               bool scaleByHalf) override;

  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}
  ID3D11Buffer* palette_buf;
  ID3D11ShaderResourceView* palette_buf_srv;
  ID3D11Buffer* palette_uniform;
  ID3D11PixelShader* palette_pixel_shader[3];
};
}
