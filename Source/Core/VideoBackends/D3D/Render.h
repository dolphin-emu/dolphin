// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
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
  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;

  void SetBlendingState(const BlendingState& state) override;
  void SetScissorRect(const MathUtil::Rectangle<int>& rc) override;
  void SetRasterizationState(const RasterizationState& state) override;
  void SetDepthState(const DepthState& state) override;
  void SetTexture(u32 index, const AbstractTexture* texture) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
  void UnbindTexture(const AbstractTexture* texture) override;
  void SetInterlacingMode() override;
  void SetViewport(float x, float y, float width, float height, float near_depth,
                   float far_depth) override;
  void SetFullscreen(bool enable_fullscreen) override;
  bool IsFullscreen() const override;

  // TODO: Fix confusing names (see ResetAPIState and RestoreAPIState)
  void ApplyState() override;
  void RestoreState() override;

  void RenderText(const std::string& text, int left, int top, u32 color) override;

  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;

  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;

  void ResetAPIState() override;
  void RestoreAPIState() override;

  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks, float Gamma) override;

  void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                   u32 color, u32 z) override;

  void ReinterpretPixelData(unsigned int convtype) override;

private:
  struct GXPipelineState
  {
    std::array<SamplerState, 8> samplers;
    BlendingState blend;
    DepthState zmode;
    RasterizationState raster;
  };

  void SetupDeviceObjects();
  void TeardownDeviceObjects();
  void Create3DVisionTexture(int width, int height);
  void CheckForSurfaceChange();
  void CheckForSurfaceResize();
  void UpdateBackbufferSize();

  void BlitScreen(TargetRectangle src, TargetRectangle dst, D3DTexture2D* src_texture,
                  u32 src_width, u32 src_height, float Gamma);

  StateCache m_state_cache;
  GXPipelineState m_gx_state;

  std::array<ID3D11BlendState*, 4> m_clear_blend_states{};
  std::array<ID3D11DepthStencilState*, 3> m_clear_depth_states{};
  ID3D11BlendState* m_reset_blend_state = nullptr;
  ID3D11DepthStencilState* m_reset_depth_state = nullptr;
  ID3D11RasterizerState* m_reset_rast_state = nullptr;

  ID3D11Texture2D* m_screenshot_texture = nullptr;
  D3DTexture2D* m_3d_vision_texture = nullptr;

  u32 m_last_multisamples = 1;
  bool m_last_stereo_mode = false;
  bool m_last_fullscreen_state = false;
};
}
