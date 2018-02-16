// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/MathUtil.h"
#include "VideoCommon/RenderBase.h"

namespace Metal
{
class StreamBuffer;
class MetalFramebuffer;
class MetalTexture;
class MetalVertexFormat;

class Renderer : public ::Renderer
{
public:
  Renderer(std::unique_ptr<MetalFramebuffer> backbuffer);
  ~Renderer() override;

  static Renderer* GetInstance() { return static_cast<Renderer*>(g_renderer.get()); }
  bool Initialize() override;
  void Shutdown() override;

  // Sets initial state, including GX.
  void SetInitialState();

  // Submit the current command buffer, and invalidate any cached state.
  void SubmitCommandBuffer(bool wait_for_completion = false);

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(const AbstractTexture* color_attachment,
                    const AbstractTexture* depth_attachment) override;
  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage, const char* source,
                                                         size_t length) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config) override;

  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override { return 0; }
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override {}
  u16 BBoxRead(int index) override { return 0; }
  void BBoxWrite(int index, u16 value) override {}
  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks, float Gamma) override;

  void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                   u32 color, u32 z) override;

  void ReinterpretPixelData(unsigned int convtype) override {}
  void SetTexture(u32 index, const AbstractTexture* texture) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
  void SetPipeline(const AbstractPipeline* pipeline) override;
  void SetFramebuffer(const AbstractFramebuffer* framebuffer) override;
  void SetAndDiscardFramebuffer(const AbstractFramebuffer* framebuffer) override;
  void SetAndClearFramebuffer(const AbstractFramebuffer* framebuffer,
                              const ClearColor& color_value = {},
                              float depth_value = 0.0f) override;
  void SetViewport(float x, float y, float width, float height, float near_depth,
                   float far_depth) override;
  void SetScissorRect(const MathUtil::Rectangle<int>& rc) override;

  void ResetAPIState() override;
  void RestoreAPIState() override;

  void DrawUtilityPipeline(const void* uniforms, u32 uniforms_size, const void* vertices,
                           u32 vertex_stride, u32 num_vertices) override;

private:
  // Draws a clear quad to the current framebuffer.
  void DrawClearQuad(const MathUtil::Rectangle<int>& rc, bool clear_color, bool clear_alpha,
                     bool clear_depth, u32 color_val = 0, float depth_val = 0.0f);

  // Draw the frame, as well as the OSD to the swap chain.
  void DrawScreen(const MetalTexture* xfb_texture, const EFBRectangle& xfb_region);

  // Copies/scales an image to the currently-bound framebuffer.
  void BlitScreen(const TargetRectangle& dst_rect, const TargetRectangle& src_rect,
                  const MetalTexture* src_tex);

  // Check for and apply config changes.
  void CheckForConfigChanges();
  void ResetSamplerStates();

  std::unique_ptr<MetalFramebuffer> m_backbuffer;

  // Streaming buffer for utility vertices.
  std::unique_ptr<StreamBuffer> m_utility_vertex_buffer;
};
}  // namespace Metal
