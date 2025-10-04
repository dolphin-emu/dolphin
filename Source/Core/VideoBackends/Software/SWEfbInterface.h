// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "VideoCommon/EFBInterface.h"
#include "VideoCommon/PerfQueryBase.h"

namespace EfbInterface
{
// xfb color format - packed so the compiler doesn't mess with alignment
#pragma pack(push, 1)
struct yuv422_packed
{
  u8 Y;
  u8 UV;
};
#pragma pack(pop)

// But this struct is only used internally, so we could optimise alignment
struct yuv444
{
  u8 Y;
  s8 U;
  s8 V;
};

enum
{
  ALP_C,
  BLU_C,
  GRN_C,
  RED_C
};

// color order is ABGR in order to emulate RGBA on little-endian hardware

// does full blending of an incoming pixel
void BlendTev(u16 x, u16 y, u8* color);

// compare z at location x,y
// writes it if it passes
// returns result of compare.
bool ZCompare(u16 x, u16 y, u32 z);

// sets the color and alpha
void SetColor(u16 x, u16 y, u8* color);
void SetDepth(u16 x, u16 y, u32 depth);

u8* GetPixelPointer(u16 x, u16 y, bool depth);

void EncodeXFB(u8* xfb_in_ram, u32 memory_stride, const MathUtil::Rectangle<int>& source_rect,
    float y_scale, float gamma);

u32 GetPerfQueryResult(PerfQueryType type);
void ResetPerfQuery();
void IncPerfCounterQuadCount(PerfQueryType type);
}  // namespace EfbInterface

namespace SW
{
class SWEFBInterface final : public EFBInterfaceBase
{
  void ReinterpretPixelData(EFBReinterpretType convtype) override;

  void PokeColor(u16 x, u16 y, u32 color) override;
  void PokeDepth(u16 x, u16 y, u32 depth) override;

  u32 PeekColorInternal(u16 x, u16 y) override;
  u32 PeekDepthInternal(u16 x, u16 y) override;
};
}  // namespace SW
