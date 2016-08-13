// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

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
  TextureCache();
  ~TextureCache();

  bool Initialize(StateTracker* state_tracker);

  bool CompileShaders() override;
  void DeleteShaders() override;
  void ConvertTexture(TCacheEntryBase* base_entry, TCacheEntryBase* base_unconverted, void* palette,
                      TlutFormat format) override;

  void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
               u32 memory_stride, PEControl::PixelFormat src_format, const EFBRectangle& src_rect,
               bool is_intensity, bool scale_by_half) override;

private:
  struct TCacheEntry : TCacheEntryBase
  {
    TCacheEntry(const TCacheEntryConfig& _config, TextureCache* parent,
                std::unique_ptr<Texture2D> texture, VkFramebuffer framebuffer);
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
    TextureCache* m_parent;
    std::unique_ptr<Texture2D> m_texture;

    // If we're an EFB copy, framebuffer for drawing into.
    VkFramebuffer m_framebuffer;
  };

  TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) override;

  bool CreateRenderPasses();

  VkRenderPass GetRenderPassForTextureUpdate(const Texture2D* texture) const;

  StateTracker* m_state_tracker = nullptr;

  VkRenderPass m_initialize_render_pass = VK_NULL_HANDLE;
  VkRenderPass m_update_render_pass = VK_NULL_HANDLE;

  std::unique_ptr<StreamBuffer> m_texture_upload_buffer;

  std::unique_ptr<TextureEncoder> m_texture_encoder;

  std::unique_ptr<PaletteTextureConverter> m_palette_texture_converter;

  VkShaderModule m_copy_shader = VK_NULL_HANDLE;
  VkShaderModule m_efb_color_to_tex_shader = VK_NULL_HANDLE;
  VkShaderModule m_efb_depth_to_tex_shader = VK_NULL_HANDLE;
};

}  // namespace Vulkan
