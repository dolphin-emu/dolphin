// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "VideoCommon/AbstractTexture.h"

namespace Vulkan
{
class Texture2D;

class VKTexture final : public AbstractTexture
{
public:
  VKTexture() = delete;
  ~VKTexture();

  void Bind(unsigned int stage) override;
  bool Save(const std::string& filename, unsigned int level) override;

  void CopyRectangleFromTexture(const AbstractTexture* source,
                                const MathUtil::Rectangle<int>& srcrect,
                                const MathUtil::Rectangle<int>& dstrect) override;
  void CopyRectangleFromTexture(Texture2D* source, const MathUtil::Rectangle<int>& srcrect,
                                const MathUtil::Rectangle<int>& dstrect);
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
            size_t buffer_size) override;

  Texture2D* GetRawTexIdentifier() const;
  VkFramebuffer GetFramebuffer() const;

  static std::unique_ptr<VKTexture> Create(const TextureConfig& tex_config);

private:
  VKTexture(const TextureConfig& tex_config, std::unique_ptr<Texture2D> texture,
            VkFramebuffer framebuffer);

  // Copies the contents of a texture using vkCmdCopyImage
  void CopyTextureRectangle(const MathUtil::Rectangle<int>& dst_rect, Texture2D* src_texture,
                            const MathUtil::Rectangle<int>& src_rect);

  // Copies (and optionally scales) the contents of a texture using a framgent shader.
  void ScaleTextureRectangle(const MathUtil::Rectangle<int>& dst_rect, Texture2D* src_texture,
                             const MathUtil::Rectangle<int>& src_rect);

  std::unique_ptr<Texture2D> m_texture;
  VkFramebuffer m_framebuffer;
};

}  // namespace Vulkan
