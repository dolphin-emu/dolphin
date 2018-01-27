// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoCommon/RenderBase.h"

enum class EFBAccessType;

namespace DX11
{
class D3DTexture2D;

class Renderer : public ::Renderer
{
public:
  Renderer();
  ~Renderer() override;

  void SetBlendingState(const BlendingState& state) override;
  void SetScissorRect(const EFBRectangle& rc) override;
  void SetRasterizationState(const RasterizationState& state) override;
  void SetDepthState(const DepthState& state) override;
  void SetSamplerState(u32 index, const SamplerState& state) override;
  void SetInterlacingMode() override;
  void SetViewport() override;
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

  void AsyncTimewarpDraw() override;
  void SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc,
                u64 ticks, float Gamma) override;

  void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                   u32 color, u32 z) override;
  void SkipClearScreen(bool colorEnable, bool alphaEnable, bool zEnable) override;

  void ReinterpretPixelData(unsigned int convtype) override;

  bool CheckForResize();

  void BlitScreen(TargetRectangle src, TargetRectangle dst, D3DTexture2D* src_texture,
                         u32 src_width, u32 src_height, float Gamma);
};
}
