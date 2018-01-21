// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "VideoCommon/AbstractFramebuffer.h"
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

  void CopyRectangleFromTexture(const AbstractTexture* src,
                                const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                u32 dst_layer, u32 dst_level) override;
  void ScaleRectangleFromTexture(const AbstractTexture* source,
                                 const MathUtil::Rectangle<int>& src_rect,
                                 const MathUtil::Rectangle<int>& dst_rect) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                          u32 layer, u32 level) override;

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

class VKFramebuffer final : public AbstractFramebuffer
{
public:
  VKFramebuffer(const VKTexture* color_attachment, const VKTexture* depth_attachment, u32 width,
                u32 height, u32 layers, u32 samples, VkFramebuffer fb,
                VkRenderPass load_render_pass, VkRenderPass discard_render_pass,
                VkRenderPass clear_render_pass);
  ~VKFramebuffer() override;

  VkFramebuffer GetFB() const { return m_fb; }
  VkRenderPass GetLoadRenderPass() const { return m_load_render_pass; }
  VkRenderPass GetDiscardRenderPass() const { return m_discard_render_pass; }
  VkRenderPass GetClearRenderPass() const { return m_clear_render_pass; }
  void TransitionForRender() const;
  void TransitionForSample() const;

  static std::unique_ptr<VKFramebuffer> Create(const VKTexture* color_attachments,
                                               const VKTexture* depth_attachment);

protected:
  const VKTexture* m_color_attachment;
  const VKTexture* m_depth_attachment;
  VkFramebuffer m_fb;
  VkRenderPass m_load_render_pass;
  VkRenderPass m_discard_render_pass;
  VkRenderPass m_clear_render_pass;
};

}  // namespace Vulkan
