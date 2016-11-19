// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
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
  Texture2D* GetEncodingTexture() const { return m_encoding_texture.get(); }
  VkFramebuffer GetEncodingTextureFramebuffer() const { return m_encoding_texture_framebuffer; }
  StagingTexture2D* GetDownloadTexture() const { return m_download_texture.get(); }
  bool Initialize();

  void ConvertTexture(VkCommandBuffer command_buffer, VkRenderPass render_pass,
                      VkFramebuffer dst_framebuffer, Texture2D* src_texture, u32 width, u32 height,
                      void* palette, TlutFormat format, u32 src_format);

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

  bool CreateUniformBuffer();
  bool CompilePaletteConversionShaders();
  bool CompileEncodingShaders();
  bool CreateEncodingRenderPass();
  bool CreateEncodingTexture();
  bool CreateDownloadTexture();

  std::array<VkShaderModule, NUM_PALETTE_CONVERSION_SHADERS> m_palette_conversion_shaders = {};

  std::unique_ptr<StreamBuffer> m_palette_stream_buffer;
  VkBufferView m_palette_buffer_view = VK_NULL_HANDLE;

  std::unique_ptr<StreamBuffer> m_uniform_buffer;

  std::array<VkShaderModule, NUM_TEXTURE_ENCODING_SHADERS> m_texture_encoding_shaders = {};

  VkRenderPass m_encoding_render_pass = VK_NULL_HANDLE;

  std::unique_ptr<Texture2D> m_encoding_texture;
  VkFramebuffer m_encoding_texture_framebuffer = VK_NULL_HANDLE;

  std::unique_ptr<StagingTexture2D> m_download_texture;
};

}  // namespace Vulkan
