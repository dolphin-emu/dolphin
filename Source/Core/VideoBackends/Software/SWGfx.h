// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "SWOGLWindow.h"
#include "VideoCommon/AbstractGfx.h"

// Normally we should be able to simply forward declare SWOGLWindow, but that ends up causing issues
// with clang due to the virtual default deconstructor in AbstractGfx and the constexpr unique_ptr
// in c++23, see https://godbolt.org/z/rPeje78e8

namespace SW
{
class SWGfx final : public AbstractGfx
{
public:
  SWGfx(std::unique_ptr<SWOGLWindow> window);

  bool IsHeadless() const override;
  virtual bool SupportsUtilityDrawing() const override;

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

  void ShowImage(const AbstractTexture* source_texture,
                 const MathUtil::Rectangle<int>& source_rc) override;

  void ScaleTexture(AbstractFramebuffer* dst_framebuffer, const MathUtil::Rectangle<int>& dst_rect,
                    const AbstractTexture* src_texture,
                    const MathUtil::Rectangle<int>& src_rect) override;

  void SetScissorRect(const MathUtil::Rectangle<int>& rc) override;

  void ClearRegion(const MathUtil::Rectangle<int>& target_rc, bool colorEnable, bool alphaEnable,
                   bool zEnable, u32 color, u32 z) override;

  SurfaceInfo GetSurfaceInfo() const override;

private:
  std::unique_ptr<SWOGLWindow> m_window;
};

}  // namespace SW
