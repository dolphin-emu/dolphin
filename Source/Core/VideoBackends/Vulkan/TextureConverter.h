// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <memory>
#include <utility>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

class AbstractTexture;
class AbstractStagingTexture;

namespace Vulkan
{
class Texture2D;
class VKTexture;

class TextureConverter
{
public:
  TextureConverter();
  ~TextureConverter();

  bool Initialize();

  // Applies palette to dst_entry, using indices from src_entry.
  void ConvertTexture(TextureCacheBase::TCacheEntry* dst_entry,
                      TextureCache::TCacheEntry* src_entry, const void* palette,
                      TLUTFormat palette_format);

  // Uses an encoding shader to copy src_texture to dest.
  void EncodeTextureToMemory(
      VkImageView src_texture, AbstractStagingTexture* dest, const EFBCopyParams& params,
      u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
      const EFBRectangle& src_rect, bool scale_by_half, float y_scale, float gamma, bool clamp_top,
      bool clamp_bottom, const TextureCacheBase::CopyFilterCoefficientArray& filter_coefficients);

  bool SupportsTextureDecoding(TextureFormat format, TLUTFormat palette_format);
  void DecodeTexture(VkCommandBuffer command_buffer, TextureCache::TCacheEntry* entry,
                     u32 dst_level, const u8* data, size_t data_size, TextureFormat format,
                     u32 width, u32 height, u32 aligned_width, u32 aligned_height, u32 row_stride,
                     const u8* palette, TLUTFormat palette_format);

private:
  static const size_t NUM_PALETTE_CONVERSION_SHADERS = 3;

  // Maximum size of a texture based on BP registers.
  static const u32 DECODING_TEXTURE_WIDTH = 1024;
  static const u32 DECODING_TEXTURE_HEIGHT = 1024;

  bool CreateTexelBuffer();
  VkBufferView CreateTexelBufferView(VkFormat format) const;

  bool CompilePaletteConversionShaders();

  VkShaderModule CompileEncodingShader(const EFBCopyParams& params);
  VkShaderModule GetEncodingShader(const EFBCopyParams& params);

  bool CreateEncodingTexture();
  bool CreateDecodingTexture();

  // Allocates storage in the texel command buffer of the specified size.
  // If the buffer does not have enough space, executes the current command buffer and tries again.
  // If this is done, g_command_buffer_mgr->GetCurrentCommandBuffer() will return a different value,
  // so it always should be re-obtained after calling this method.
  // Once the data copy is done, call m_texel_buffer->CommitMemory(size).
  bool ReserveTexelBufferStorage(size_t size, size_t alignment);

  // Returns the command buffer that the texture conversion should occur in for the given texture.
  // This can be the initialization/copy command buffer, or the drawing command buffer.
  VkCommandBuffer GetCommandBufferForTextureConversion(const TextureCache::TCacheEntry* src_entry);

  // Shared between conversion types
  std::unique_ptr<StreamBuffer> m_texel_buffer;
  VkBufferView m_texel_buffer_view_r8_uint = VK_NULL_HANDLE;
  VkBufferView m_texel_buffer_view_r16_uint = VK_NULL_HANDLE;
  VkBufferView m_texel_buffer_view_r32g32_uint = VK_NULL_HANDLE;
  VkBufferView m_texel_buffer_view_rgba8_uint = VK_NULL_HANDLE;
  VkBufferView m_texel_buffer_view_rgba8_unorm = VK_NULL_HANDLE;
  size_t m_texel_buffer_size = 0;

  // Palette conversion - taking an indexed texture and applying palette
  std::array<VkShaderModule, NUM_PALETTE_CONVERSION_SHADERS> m_palette_conversion_shaders = {};

  // Texture encoding - RGBA8->GX format in memory
  std::map<EFBCopyParams, VkShaderModule> m_encoding_shaders;
  std::unique_ptr<AbstractTexture> m_encoding_render_texture;

  // Texture decoding - GX format in memory->RGBA8
  struct TextureDecodingPipeline
  {
    const TextureConversionShaderTiled::DecodingShaderInfo* base_info;
    VkShaderModule compute_shader;
    bool valid;
  };
  std::map<std::pair<TextureFormat, TLUTFormat>, TextureDecodingPipeline> m_decoding_pipelines;
  std::unique_ptr<Texture2D> m_decoding_texture;
};

}  // namespace Vulkan
