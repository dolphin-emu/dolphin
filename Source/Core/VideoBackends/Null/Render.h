// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/RenderBase.h"

namespace Null
{
class Renderer : public ::Renderer
{
public:
  Renderer();
  ~Renderer() override;

  void RenderText(const std::string& pstr, int left, int top, u32 color) override;
  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override { return 0; }
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override {}
  u16 BBoxRead(int index) override { return 0; }
  void BBoxWrite(int index, u16 value) override {}
  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void AsyncTimewarpDraw() override
  {
  }

  void SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc,
                u64 ticks, float gamma) override;

  void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                   u32 color, u32 z) override
  {
  }
  void SkipClearScreen(bool colorEnable, bool alphaEnable, bool zEnable) override
  {
  }

  void ReinterpretPixelData(unsigned int convtype) override {}
};
}
