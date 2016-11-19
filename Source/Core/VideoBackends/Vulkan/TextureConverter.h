// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

namespace Vulkan
{
class StagingTexture2D;
class Texture2D;

class TextureConverter
{
public:
  TextureConverter();
  ~TextureConverter();

  VkRenderPass GetEncodingRenderPass() const { return m_encoding_render_pass; }
  Texture2D* GetEncodingTexture() const { return m_encoding_render_texture.get(); }
  VkFramebuffer GetEncodingTextureFramebuffer() const { return m_encoding_render_framebuffer; }
  StagingTexture2D* GetDownloadTexture() const { return m_encoding_download_texture.get(); }
  bool Initialize();

  // Applies palette to dst_entry, using indices from src_entry.
  void ConvertTexture(TextureCache::TCacheEntry* dst_entry, TextureCache::TCacheEntry* src_entry,
                      VkRenderPass render_pass, const void* palette, TlutFormat palette_format);

  // Uses an encoding shader to copy src_texture to dest_ptr.
  // NOTE: Executes the current command buffer.
  void EncodeTextureToMemory(VkImageView src_texture, u8* dest_ptr, u32 format, u32 native_width,
                             u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                             PEControl::PixelFormat src_format, bool is_intensity,
                             int scale_by_half, const EFBRectangle& source);

private:
  static const u32 NUM_TEXTURE_ENCODING_SHADERS = 64;
  static const u32 ENCODING_TEXTURE_WIDTH = EFB_WIDTH * 4;
  static const u32 ENCODING_TEXTURE_HEIGHT = 1024;
  static const VkFormat ENCODING_TEXTURE_FORMAT = VK_FORMAT_B8G8R8A8_UNORM;
  static const size_t NUM_PALETTE_CONVERSION_SHADERS = 3;

  bool CreateTexelBuffer();
  VkBufferView CreateTexelBufferView(VkFormat format) const;

  bool CompilePaletteConversionShaders();

  bool CompileEncodingShaders();
  bool CreateEncodingRenderPass();
  bool CreateEncodingTexture();
  bool CreateEncodingDownloadTexture();

  // Shared between conversion types
  std::unique_ptr<StreamBuffer> m_texel_buffer;
  VkBufferView m_texel_buffer_view_r16_uint = VK_NULL_HANDLE;
  size_t m_texel_buffer_size = 0;

  // Palette conversion - taking an indexed texture and applying palette
  std::array<VkShaderModule, NUM_PALETTE_CONVERSION_SHADERS> m_palette_conversion_shaders = {};

  // Texture encoding - RGBA8->GX format in memory
  std::array<VkShaderModule, NUM_TEXTURE_ENCODING_SHADERS> m_encoding_shaders = {};
  VkRenderPass m_encoding_render_pass = VK_NULL_HANDLE;
  std::unique_ptr<Texture2D> m_encoding_render_texture;
  VkFramebuffer m_encoding_render_framebuffer = VK_NULL_HANDLE;
  std::unique_ptr<StagingTexture2D> m_encoding_download_texture;
};

}  // namespace Vulkan
