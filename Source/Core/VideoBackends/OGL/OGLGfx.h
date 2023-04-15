// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Constants.h"

class GLContext;

namespace OGL
{
class OGLFramebuffer;
class OGLTexture;

class OGLGfx final : public AbstractGfx
{
public:
  OGLGfx(std::unique_ptr<GLContext> main_gl_context, float backbuffer_scale);
  ~OGLGfx();

  bool IsHeadless() const override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config,
                                                 std::string_view name) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
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
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment) override;

  void SetPipeline(const AbstractPipeline* pipeline) override;
  void SetFramebuffer(AbstractFramebuffer* framebuffer) override;
  void SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer) override;
  void SetAndClearFramebuffer(AbstractFramebuffer* framebuffer, const ClearColor& color_value = {},
                              float depth_value = 0.0f) override;
  void ClearRegion(const MathUtil::Rectangle<int>& target_rc, bool colorEnable, bool alphaEnable,
                   bool zEnable, u32 color, u32 z) override;
  void SetScissorRect(const MathUtil::Rectangle<int>& rc) override;
  void SetTexture(u32 index, const AbstractTexture* texture) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
  void SetComputeImageTexture(AbstractTexture* texture, bool read, bool write) override;
  void UnbindTexture(const AbstractTexture* texture) override;
  void SetViewport(float x, float y, float width, float height, float near_depth,
                   float far_depth) override;
  void Draw(u32 base_vertex, u32 num_vertices) override;
  void DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex) override;
  void DispatchComputeShader(const AbstractShader* shader, u32 groupsize_x, u32 groupsize_y,
                             u32 groupsize_z, u32 groups_x, u32 groups_y, u32 groups_z) override;
  void BindBackbuffer(const ClearColor& clear_color = {}) override;
  void PresentBackbuffer() override;

  void BeginUtilityDrawing() override;
  void EndUtilityDrawing() override;

  void Flush() override;
  void WaitForGPUIdle() override;
  void OnConfigChanged(u32 bits) override;

  virtual void SelectLeftBuffer() override;
  virtual void SelectRightBuffer() override;
  virtual void SelectMainBuffer() override;

  std::unique_ptr<VideoCommon::AsyncShaderCompiler> CreateAsyncShaderCompiler() override;

  // Only call methods from this on the GPU thread.
  GLContext* GetMainGLContext() const { return m_main_gl_context.get(); }
  bool IsGLES() const;

  // Invalidates a cached texture binding. Required for texel buffers when they borrow the units.
  void InvalidateTextureBinding(u32 index) { m_bound_textures[index] = nullptr; }

  // The shared framebuffer exists for copying textures when extensions are not available. It is
  // slower, but the only way to do these things otherwise.
  u32 GetSharedReadFramebuffer() const { return m_shared_read_framebuffer; }
  u32 GetSharedDrawFramebuffer() const { return m_shared_draw_framebuffer; }
  void BindSharedReadFramebuffer();
  void BindSharedDrawFramebuffer();

  // Restores FBO binding after it's been changed.
  void RestoreFramebufferBinding();

  SurfaceInfo GetSurfaceInfo() const override;

private:
  void CheckForSurfaceChange();
  void CheckForSurfaceResize();

  void ApplyRasterizationState(const RasterizationState state);
  void ApplyDepthState(const DepthState state);
  void ApplyBlendingState(const BlendingState state);

  std::unique_ptr<GLContext> m_main_gl_context;
  std::unique_ptr<OGLFramebuffer> m_system_framebuffer;
  std::array<const OGLTexture*, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS> m_bound_textures{};
  AbstractTexture* m_bound_image_texture = nullptr;
  RasterizationState m_current_rasterization_state;
  DepthState m_current_depth_state;
  BlendingState m_current_blend_state;
  u32 m_shared_read_framebuffer = 0;
  u32 m_shared_draw_framebuffer = 0;
  float m_backbuffer_scale;
};

inline OGLGfx* GetOGLGfx()
{
  return static_cast<OGLGfx*>(g_gfx.get());
}

}  // namespace OGL
