// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <string>
#include "VideoBackends/D3D/D3DState.h"
#include "VideoCommon/RenderBase.h"

enum class EFBAccessType;

namespace DX11
{
class D3DTexture2D;

class Renderer : public ::Renderer
{
public:
  Renderer(int backbuffer_width, int backbuffer_height);
  ~Renderer() override;

  StateCache& GetStateCache() { return m_state_cache; }

  bool IsHeadless() const override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage, const char* source,
                                                         size_t length) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(const AbstractTexture* color_attachment,
                    const AbstractTexture* depth_attachment) override;

  void SetPipeline(const AbstractPipeline* pipeline) override;
  void SetFramebuffer(const AbstractFramebuffer* framebuffer) override;
  void SetAndDiscardFramebuffer(const AbstractFramebuffer* framebuffer) override;
  void SetAndClearFramebuffer(const AbstractFramebuffer* framebuffer,
                              const ClearColor& color_value = {},
                              float depth_value = 0.0f) override;
  void SetScissorRect(const MathUtil::Rectangle<int>& rc) override;
  void SetTexture(u32 index, const AbstractTexture* texture) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
  void UnbindTexture(const AbstractTexture* texture) override;
  void SetInterlacingMode() override;
  void SetViewport(float x, float y, float width, float height, float near_depth,
                   float far_depth) override;
  void SetFullscreen(bool enable_fullscreen) override;
  bool IsFullscreen() const override;

  void RenderText(const std::string& text, int left, int top, u32 color) override;

  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;

  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;

  void ResetAPIState() override;
  void RestoreAPIState() override;

  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks) override;

  void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                   u32 color, u32 z) override;

  void ReinterpretPixelData(unsigned int convtype) override;

  void DrawUtilityPipeline(const void* uniforms, u32 uniforms_size, const void* vertices,
                           u32 vertex_stride, u32 num_vertices) override;
  void DispatchComputeShader(const AbstractShader* shader, const void* uniforms, u32 uniforms_size,
                             u32 groups_x, u32 groups_y, u32 groups_z) override;

private:
  void SetupDeviceObjects();
  void TeardownDeviceObjects();
  void Create3DVisionTexture(int width, int height);
  void CheckForSurfaceChange();
  void CheckForSurfaceResize();
  void UpdateBackbufferSize();

  void BlitScreen(TargetRectangle src, TargetRectangle dst, D3DTexture2D* src_texture,
                  u32 src_width, u32 src_height);

  void UpdateUtilityUniformBuffer(const void* uniforms, u32 uniforms_size);
  void UpdateUtilityVertexBuffer(const void* vertices, u32 vertex_stride, u32 num_vertices);

  StateCache m_state_cache;

  std::array<ID3D11BlendState*, 4> m_clear_blend_states{};
  std::array<ID3D11DepthStencilState*, 3> m_clear_depth_states{};
  ID3D11BlendState* m_reset_blend_state = nullptr;
  ID3D11DepthStencilState* m_reset_depth_state = nullptr;
  ID3D11RasterizerState* m_reset_rast_state = nullptr;

  ID3D11Texture2D* m_screenshot_texture = nullptr;
  D3DTexture2D* m_3d_vision_texture = nullptr;

  ID3D11Buffer* m_utility_vertex_buffer = nullptr;
  ID3D11Buffer* m_utility_uniform_buffer = nullptr;

  u32 m_last_multisamples = 1;
  bool m_last_stereo_mode = false;
  bool m_last_fullscreen_state = false;
};
}
