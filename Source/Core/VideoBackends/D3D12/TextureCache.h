// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/TextureCacheBase.h"

namespace DX12
{
class D3DStreamBuffer;

class TextureCache final : public TextureCacheBase
{
public:
  TextureCache();
  ~TextureCache();

  virtual void BindTextures();

private:
  std::unique_ptr<AbstractTextureBase>
  CreateTexture(const AbstractTextureBase::TextureConfig& config) override;

  void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, void* palette,
                      TlutFormat format) override;

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
               bool is_intensity, bool scale_by_half) override;

  void CompileShaders() override {}
  void DeleteShaders() override {}
  std::unique_ptr<D3DStreamBuffer> m_palette_stream_buffer;

  ID3D12Resource* m_palette_uniform_buffer = nullptr;
  D3D12_SHADER_BYTECODE m_palette_pixel_shaders[3] = {};
};
}
