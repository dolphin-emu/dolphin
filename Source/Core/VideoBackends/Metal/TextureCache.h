// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Metal/MetalFramebuffer.h"
#include "VideoBackends/Metal/MetalTexture.h"

#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConfig.h"

namespace Metal
{
class TextureCache : public TextureCacheBase
{
public:
  TextureCache() {}
  ~TextureCache() {}
  static TextureCache* GetInstance() { return static_cast<TextureCache*>(g_texture_cache.get()); }
  const MetalTexture* GetEFBColorTexture() const { return m_efb_color_texture.get(); }
  const MetalTexture* GetEFBDepthTexture() const { return m_efb_depth_texture.get(); }
  const MetalFramebuffer* GetEFBFramebuffer() const { return m_efb_framebuffer.get(); }
  bool Initialize();

  bool CompileShaders() override { return true; }
  void DeleteShaders() override {}
  void ConvertTexture(TCacheEntry* entry, TCacheEntry* unconverted, const void* palette,
                      TLUTFormat format) override
  {
  }

  void CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
               u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half) override;

  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, EFBCopyFormat dst_format,
                           bool is_intensity) override;

  void RecreateEFBFramebuffer();

private:
  // TODO: Move these to common.
  static constexpr u32 ENCODING_TEXTURE_WIDTH = EFB_WIDTH * 4;
  static constexpr u32 ENCODING_TEXTURE_HEIGHT = 1024;
  static constexpr AbstractTextureFormat ENCODING_TEXTURE_FORMAT = AbstractTextureFormat::BGRA8;

  bool CreateEFBFramebuffer();
  void DestroyEFBFramebuffer();
  bool CreateCopyTextures();

  std::unique_ptr<MetalTexture> m_efb_color_texture;
  std::unique_ptr<MetalTexture> m_efb_depth_texture;
  std::unique_ptr<MetalFramebuffer> m_efb_framebuffer;

  std::unique_ptr<MetalTexture> m_copy_render_texture;
  std::unique_ptr<MetalStagingTexture> m_copy_readback_texture;
};

}  // namespace Metal
