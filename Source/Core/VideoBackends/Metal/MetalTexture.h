// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"

namespace Metal
{
class MetalFramebuffer;

class MetalTexture final : public AbstractTexture
{
public:
  explicit MetalTexture(const TextureConfig& config, mtlpp::Texture tex,
                        std::unique_ptr<MetalFramebuffer> fb);
  ~MetalTexture();

  static std::unique_ptr<MetalTexture> Create(const TextureConfig& config);

  const mtlpp::Texture& GetTexture() const { return m_texture; }
  const MetalFramebuffer* GetFramebuffer() const { return m_framebuffer.get(); }
  bool HasFramebuffer() const { return static_cast<bool>(m_framebuffer); }
  void CopyRectangleFromTexture(const AbstractTexture* src,
                                const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                u32 dst_layer, u32 dst_level) override;
  void ScaleRectangleFromTexture(const AbstractTexture* source,
                                 const MathUtil::Rectangle<int>& srcrect,
                                 const MathUtil::Rectangle<int>& dstrect) override;
  void ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                          u32 layer, u32 level) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
            size_t buffer_size) override;

private:
  mtlpp::Texture m_texture;
  std::unique_ptr<MetalFramebuffer> m_framebuffer;
};

class MetalStagingTexture final : public AbstractStagingTexture
{
public:
  explicit MetalStagingTexture(mtlpp::Buffer& buffer, u32 stride, StagingTextureType type,
                               const TextureConfig& config);
  ~MetalStagingTexture();

  static std::unique_ptr<MetalStagingTexture> Create(StagingTextureType type,
                                                     const TextureConfig& config);

  void CopyFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& src_rect,
                       u32 src_layer, u32 src_level,
                       const MathUtil::Rectangle<int>& dst_rect) override;
  void CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                     u32 dst_level) override;

  bool Map() override;
  void Unmap() override;
  void Flush() override;

private:
  mtlpp::Buffer m_buffer;
  CommandBufferId m_pending_command_buffer_id = 0;
};

}  // namespace Metal
