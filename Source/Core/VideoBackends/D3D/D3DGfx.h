// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <d3d11.h>
#include <string_view>
#include "VideoBackends/D3D/D3DState.h"
#include "VideoCommon/AbstractGfx.h"

class BoundingBox;

namespace DX11
{
class SwapChain;
class DXTexture;
class DXFramebuffer;

class Gfx final : public ::AbstractGfx
{
public:
  Gfx(std::unique_ptr<SwapChain> swap_chain, float backbuffer_scale);
  ~Gfx() override;

  StateCache& GetStateCache() { return m_state_cache; }

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
  CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                    std::vector<AbstractTexture*> additional_color_attachments) override;

  void SetPipeline(const AbstractPipeline* pipeline) override;
  void SetFramebuffer(AbstractFramebuffer* framebuffer) override;
  void SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer) override;
  void SetAndClearFramebuffer(AbstractFramebuffer* framebuffer, const ClearColor& color_value = {},
                              float depth_value = 0.0f) override;
  void SetScissorRect(const MathUtil::Rectangle<int>& rc) override;
  void SetTexture(u32 index, const AbstractTexture* texture) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
  void SetComputeImageTexture(u32 index, AbstractTexture* texture, bool read, bool write) override;
  void UnbindTexture(const AbstractTexture* texture) override;
  void SetViewport(float x, float y, float width, float height, float near_depth,
                   float far_depth) override;
  void Draw(u32 base_vertex, u32 num_vertices) override;
  void DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex) override;
  void DispatchComputeShader(const AbstractShader* shader, u32 groupsize_x, u32 groupsize_y,
                             u32 groupsize_z, u32 groups_x, u32 groups_y, u32 groups_z) override;
  void BindBackbuffer(const ClearColor& clear_color = {}) override;
  void PresentBackbuffer() override;
  void SetFullscreen(bool enable_fullscreen) override;
  bool IsFullscreen() const override;

  void Flush(FlushType flushType) override;
  void WaitForGPUIdle() override;

  void OnConfigChanged(u32 bits) override;

  SurfaceInfo GetSurfaceInfo() const override;

private:
  void CheckForSwapChainChanges();

  StateCache m_state_cache;

  float m_backbuffer_scale;
  std::unique_ptr<SwapChain> m_swap_chain;
};
}  // namespace DX11
