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
class PaletteTextureConverter;
class StateTracker;
class Texture2D;
class TextureEncoder;

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
    void Load(unsigned int width, unsigned int height, unsigned int expanded_width,
              unsigned int level) override;
    void FromRenderTarget(u8* dst, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
                          bool scale_by_half, unsigned int cbufid, const float* colmat) override;
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

  bool Initialize();

  bool CompileShaders() override;
  void DeleteShaders() override;

  TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

  void ConvertTexture(TCacheEntryBase* base_entry, TCacheEntryBase* base_unconverted, void* palette,
                      TlutFormat format) override;

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
               bool is_intensity, bool scale_by_half) override;

  void CopyRectangleFromTexture(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
                                Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

  // Encodes texture to guest memory in XFB (YUYV) format.
  void EncodeYUYVTextureToMemory(void* dst_ptr, u32 dst_width, u32 dst_stride, u32 dst_height,
                                 Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

  // Decodes data from guest memory in XFB (YUYV) format to a RGBA format texture on the GPU.
  void DecodeYUYVTextureFromMemory(TCacheEntry* dst_texture, const void* src_ptr, u32 src_width,
                                   u32 src_stride, u32 src_height);

private:
  bool CreateRenderPasses();
  VkRenderPass GetRenderPassForTextureUpdate(const Texture2D* texture) const;

  // Copies the contents of a texture using vkCmdCopyImage
  void CopyTextureRectangle(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
                            Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

  // Copies (and optionally scales) the contents of a texture using a framgent shader.
  void ScaleTextureRectangle(TCacheEntry* dst_texture, const MathUtil::Rectangle<int>& dst_rect,
                             Texture2D* src_texture, const MathUtil::Rectangle<int>& src_rect);

  VkRenderPass m_initialize_render_pass = VK_NULL_HANDLE;
  VkRenderPass m_update_render_pass = VK_NULL_HANDLE;

  std::unique_ptr<StreamBuffer> m_texture_upload_buffer;

  std::unique_ptr<TextureEncoder> m_texture_encoder;

  std::unique_ptr<PaletteTextureConverter> m_palette_texture_converter;

  VkShaderModule m_copy_shader = VK_NULL_HANDLE;
  VkShaderModule m_efb_color_to_tex_shader = VK_NULL_HANDLE;
  VkShaderModule m_efb_depth_to_tex_shader = VK_NULL_HANDLE;
  VkShaderModule m_rgb_to_yuyv_shader = VK_NULL_HANDLE;
  VkShaderModule m_yuyv_to_rgb_shader = VK_NULL_HANDLE;
};

}  // namespace Vulkan
