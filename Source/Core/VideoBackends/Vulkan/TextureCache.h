// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoCommon/TextureCacheBase.h"

namespace Vulkan
{
class TextureConverter;
class StateTracker;
class Texture2D;

class TextureCache : public TextureCacheBase
{
public:
  struct TCacheEntry : TCacheEntryBase
  {
    TCacheEntry(const TCacheEntryConfig& config_, std::unique_ptr<Texture2D> texture,
                VkFramebuffer framebuffer);
    ~TCacheEntry();

    Texture2D* GetTexture() const { return m_texture.get(); }
    VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
    void Load(const u8* buffer, unsigned int width, unsigned int height,
              unsigned int expanded_width, unsigned int level) override;
    void FromRenderTarget(bool is_depth_copy, const EFBRectangle& src_rect, bool scale_by_half,
                          unsigned int cbufid, const float* colmat) override;
    void CopyRectangleFromTexture(const TCacheEntryBase* source,
                                  const MathUtil::Rectangle<int>& src_rect,
                                  const MathUtil::Rectangle<int>& dst_rect) override;

    void Bind(unsigned int stage) override;
    bool Save(const std::string& filename, unsigned int level) override;

  private:
    std::unique_ptr<Texture2D> m_texture;
    VkFramebuffer m_framebuffer;
  };

  TextureCache();
  ~TextureCache();

  static TextureCache* GetInstance();

  TextureConverter* GetTextureConverter() const { return m_texture_converter.get(); }
  bool Initialize();

  bool CompileShaders() override;
  void DeleteShaders() override;

  TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

  void ConvertTexture(TCacheEntryBase* base_entry, TCacheEntryBase* base_unconverted, void* palette,
                      TlutFormat format) override;

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, bool is_depth_copy, const EFBRectangle& src_rect,
               bool is_intensity, bool scale_by_half) override;

  void CopyRectangleFromTexture(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
                                Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

  bool SupportsGPUTextureDecode(TextureFormat format, TlutFormat palette_format) override;

  void DecodeTextureOnGPU(TCacheEntryBase* entry, u32 dst_level, const u8* data, size_t data_size,
                          TextureFormat format, u32 width, u32 height, u32 aligned_width,
                          u32 aligned_height, u32 row_stride, const u8* palette,
                          TlutFormat palette_format) override;

private:
  bool CreateRenderPasses();

  // Copies the contents of a texture using vkCmdCopyImage
  void CopyTextureRectangle(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
                            Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

  // Copies (and optionally scales) the contents of a texture using a framgent shader.
  void ScaleTextureRectangle(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
                             Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

  VkRenderPass m_render_pass = VK_NULL_HANDLE;

  std::unique_ptr<StreamBuffer> m_texture_upload_buffer;

  std::unique_ptr<TextureConverter> m_texture_converter;

  VkShaderModule m_copy_shader = VK_NULL_HANDLE;
  VkShaderModule m_efb_color_to_tex_shader = VK_NULL_HANDLE;
  VkShaderModule m_efb_depth_to_tex_shader = VK_NULL_HANDLE;
};

}  // namespace Vulkan
