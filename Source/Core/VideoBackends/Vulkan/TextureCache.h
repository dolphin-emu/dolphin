// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConverterShaderGen.h"

namespace Vulkan
{
class TextureConverter;
class StateTracker;
class Texture2D;
class VKTexture;

class TextureCache : public TextureCacheBase
{
public:
  TextureCache();
  ~TextureCache();

  static TextureCache* GetInstance();

  TextureConverter* GetTextureConverter() const { return m_texture_converter.get(); }
  bool Initialize();

  bool CompileShaders() override;
  void DeleteShaders() override;

  void ConvertTexture(TCacheEntry* destination, TCacheEntry* source, const void* palette,
                      TLUTFormat format) override;

  void CopyEFB(AbstractStagingTexture* dst, const EFBCopyParams& params, u32 native_width,
               u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
               bool scale_by_half, float y_scale, float gamma, bool clamp_top, bool clamp_bottom,
               const CopyFilterCoefficientArray& filter_coefficients) override;

  bool SupportsGPUTextureDecode(TextureFormat format, TLUTFormat palette_format) override;

  void DecodeTextureOnGPU(TCacheEntry* entry, u32 dst_level, const u8* data, size_t data_size,
                          TextureFormat format, u32 width, u32 height, u32 aligned_width,
                          u32 aligned_height, u32 row_stride, const u8* palette,
                          TLUTFormat palette_format) override;

  VkShaderModule GetCopyShader() const;
  StreamBuffer* GetTextureUploadBuffer() const;

private:
  void CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy, const EFBRectangle& src_rect,
                           bool scale_by_half, EFBCopyFormat dst_format, bool is_intensity,
                           float gamma, bool clamp_top, bool clamp_bottom,
                           const CopyFilterCoefficientArray& filter_coefficients) override;

  std::unique_ptr<StreamBuffer> m_texture_upload_buffer;

  std::unique_ptr<TextureConverter> m_texture_converter;

  VkShaderModule m_copy_shader = VK_NULL_HANDLE;
  std::map<TextureConversionShaderGen::TCShaderUid, VkShaderModule> m_efb_copy_to_tex_shaders;
};

}  // namespace Vulkan
