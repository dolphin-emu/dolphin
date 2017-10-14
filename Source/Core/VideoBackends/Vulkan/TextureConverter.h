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

namespace Vulkan
{
class StagingTexture2D;
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
                      TextureCache::TCacheEntry* src_entry, VkRenderPass render_pass,
                      const void* palette, TLUTFormat palette_format);

  // Uses an encoding shader to copy src_texture to dest_ptr.
  // NOTE: Executes the current command buffer.
  void EncodeTextureToMemory(VkImageView src_texture, u8* dest_ptr, const EFBCopyParams& params,
                             u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
                             u32 memory_stride, const EFBRectangle& src_rect, bool scale_by_half);

  // Encodes texture to guest memory in XFB (YUYV) format.
  void EncodeTextureToMemoryYUYV(void* dst_ptr, u32 dst_width, u32 dst_stride, u32 dst_height,
                                 Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

  // Decodes data from guest memory in XFB (YUYV) format to a RGBA format texture on the GPU.
  void DecodeYUYVTextureFromMemory(VKTexture* dst_texture, const void* src_ptr, u32 src_width,
                                   u32 src_stride, u32 src_height);

  bool SupportsTextureDecoding(TextureFormat format, TLUTFormat palette_format);
  void DecodeTexture(VkCommandBuffer command_buffer, TextureCache::TCacheEntry* entry,
                     u32 dst_level, const u8* data, size_t data_size, TextureFormat format,
                     u32 width, u32 height, u32 aligned_width, u32 aligned_height, u32 row_stride,
                     const u8* palette, TLUTFormat palette_format);

private:
  static const u32 ENCODING_TEXTURE_WIDTH = EFB_WIDTH * 4;
  static const u32 ENCODING_TEXTURE_HEIGHT = 1024;
  static const VkFormat ENCODING_TEXTURE_FORMAT = VK_FORMAT_B8G8R8A8_UNORM;
  static const size_t NUM_PALETTE_CONVERSION_SHADERS = 3;

  // Maximum size of a texture based on BP registers.
  static const u32 DECODING_TEXTURE_WIDTH = 1024;
  static const u32 DECODING_TEXTURE_HEIGHT = 1024;

  bool CreateTexelBuffer();
  VkBufferView CreateTexelBufferView(VkFormat format) const;

  bool CompilePaletteConversionShaders();

  VkShaderModule CompileEncodingShader(const EFBCopyParams& params);
  VkShaderModule GetEncodingShader(const EFBCopyParams& params);

  bool CreateEncodingRenderPass();
  bool CreateEncodingTexture();
  bool CreateEncodingDownloadTexture();

  bool CreateDecodingTexture();

  bool CompileYUYVConversionShaders();

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
  VkBufferView m_texel_buffer_view_rgba8_unorm = VK_NULL_HANDLE;
  size_t m_texel_buffer_size = 0;

  // Palette conversion - taking an indexed texture and applying palette
  std::array<VkShaderModule, NUM_PALETTE_CONVERSION_SHADERS> m_palette_conversion_shaders = {};

  // Texture encoding - RGBA8->GX format in memory
  std::map<EFBCopyParams, VkShaderModule> m_encoding_shaders;
  VkRenderPass m_encoding_render_pass = VK_NULL_HANDLE;
  std::unique_ptr<Texture2D> m_encoding_render_texture;
  VkFramebuffer m_encoding_render_framebuffer = VK_NULL_HANDLE;
  std::unique_ptr<StagingTexture2D> m_encoding_download_texture;

  // Texture decoding - GX format in memory->RGBA8
  struct TextureDecodingPipeline
  {
    const TextureConversionShader::DecodingShaderInfo* base_info;
    VkShaderModule compute_shader;
    bool valid;
  };
  std::map<std::pair<TextureFormat, TLUTFormat>, TextureDecodingPipeline> m_decoding_pipelines;
  std::unique_ptr<Texture2D> m_decoding_texture;

  // XFB encoding/decoding shaders
  VkShaderModule m_rgb_to_yuyv_shader = VK_NULL_HANDLE;
  VkShaderModule m_yuyv_to_rgb_shader = VK_NULL_HANDLE;
};

}  // namespace Vulkan
