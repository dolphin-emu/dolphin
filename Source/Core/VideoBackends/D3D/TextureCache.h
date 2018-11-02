// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConverterShaderGen.h"

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
  void ConvertTexture(TCacheEntry* destination, TCacheEntry* source, const void* palette,
                      TLUTFormat format) override;

  void CopyEFB(AbstractStagingTexture* dst, const EFBCopyParams& params, u32 native_width,
               u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half, float y_scale, float gamma, bool clamp_top, bool clamp_bottom,
               const CopyFilterCoefficientArray& filter_coefficients) override;

  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, EFBCopyFormat dst_format, bool is_intensity,
                           float gamma, bool clamp_top, bool clamp_bottom,
                           const CopyFilterCoefficientArray& filter_coefficients) override;

  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}
  ID3D11PixelShader* GetEFBToTexPixelShader(const TextureConversionShaderGen::TCShaderUid& uid);

  ID3D11Buffer* palette_buf;
  ID3D11ShaderResourceView* palette_buf_srv;
  ID3D11Buffer* uniform_buffer;
  ID3D11PixelShader* palette_pixel_shader[3];

  std::map<TextureConversionShaderGen::TCShaderUid, ID3D11PixelShader*> m_efb_to_tex_pixel_shaders;
};
}
