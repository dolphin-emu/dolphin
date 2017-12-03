// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"

namespace Vulkan
{
class StagingBuffer;
class Texture2D;

class VKTexture final : public AbstractTexture
{
public:
  VKTexture() = delete;
  ~VKTexture();

  void Bind(unsigned int stage) override;

  void CopyRectangleFromTexture(const AbstractTexture* src,
                                const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                u32 dst_layer, u32 dst_level) override;
  void ScaleRectangleFromTexture(const AbstractTexture* source,
                                 const MathUtil::Rectangle<int>& src_rect,
                                 const MathUtil::Rectangle<int>& dst_rect);

  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
            size_t buffer_size) override;

  Texture2D* GetRawTexIdentifier() const;
  VkFramebuffer GetFramebuffer() const;

  static std::unique_ptr<VKTexture> Create(const TextureConfig& tex_config);

private:
  VKTexture(const TextureConfig& tex_config, std::unique_ptr<Texture2D> texture,
            VkFramebuffer framebuffer);

  std::unique_ptr<Texture2D> m_texture;
  VkFramebuffer m_framebuffer;
};

class VKStagingTexture final : public AbstractStagingTexture
{
public:
  VKStagingTexture() = delete;
  ~VKStagingTexture();

  void CopyFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& src_rect,
                       u32 src_layer, u32 src_level,
                       const MathUtil::Rectangle<int>& dst_rect) override;
  void CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                     u32 dst_level) override;

  bool Map() override;
  void Unmap() override;
  void Flush() override;

  // This overload is provided for compatibility as we dropped StagingTexture2D.
  // For now, FramebufferManager relies on them. But we can drop it once we move that to common.
  void CopyFromTexture(Texture2D* src, const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                       u32 src_level, const MathUtil::Rectangle<int>& dst_rect);

  static std::unique_ptr<VKStagingTexture> Create(StagingTextureType type,
                                                  const TextureConfig& config);

private:
  VKStagingTexture(StagingTextureType type, const TextureConfig& config,
                   std::unique_ptr<StagingBuffer> buffer);

  std::unique_ptr<StagingBuffer> m_staging_buffer;
  VkFence m_flush_fence = VK_NULL_HANDLE;
};

}  // namespace Vulkan
