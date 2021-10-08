// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string_view>

#include "Common/CommonTypes.h"

#include "VideoCommon/RenderBase.h"

class BoundingBox;
class SWOGLWindow;

namespace SW
{
class SWRenderer final : public Renderer
{
public:
  SWRenderer(std::unique_ptr<SWOGLWindow> window);

  bool IsHeadless() const override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config,
                                                 std::string_view name) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment) override;

  void BindBackbuffer(const ClearColor& clear_color = {}) override;

  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage, std::string_view source,
                                                         std::string_view name) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length,
                                                         std::string_view name) override;
  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config,
                                                   const void* cache_data = nullptr,
                                                   size_t cache_data_length = 0) override;

  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override {}

  void RenderXFBToScreen(const MathUtil::Rectangle<int>& target_rc,
                         const AbstractTexture* source_texture,
                         const MathUtil::Rectangle<int>& source_rc) override;

  void ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                   bool zEnable, u32 color, u32 z) override;

  void ReinterpretPixelData(EFBReinterpretType convtype) override {}

  void ScaleTexture(AbstractFramebuffer* dst_framebuffer, const MathUtil::Rectangle<int>& dst_rect,
                    const AbstractTexture* src_texture,
                    const MathUtil::Rectangle<int>& src_rect) override;

protected:
  std::unique_ptr<BoundingBox> CreateBoundingBox() const override;

private:
  std::unique_ptr<SWOGLWindow> m_window;
};
}  // namespace SW
