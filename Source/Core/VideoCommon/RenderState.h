// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BPStructs.h"

enum class PrimitiveType : u32
{
  Points,
  Lines,
  Triangles,
  TriangleStrip,
};

union RasterizationState
{
  void Generate(const BPMemory& bp, PrimitiveType primitive_type);

  BitField<0, 2, GenMode::CullMode> cullmode;
  BitField<3, 2, PrimitiveType> primitive;

  u32 hex;
};

union DepthState
{
  void Generate(const BPMemory& bp);

  BitField<0, 1, u32> testenable;
  BitField<1, 1, u32> updateenable;
  BitField<2, 3, ZMode::CompareMode> func;

  u32 hex;
};

union BlendingState
{
  void Generate(const BPMemory& bp);

  BitField<0, 1, u32> blendenable;
  BitField<1, 1, u32> logicopenable;
  BitField<2, 1, u32> dstalpha;
  BitField<3, 1, u32> colorupdate;
  BitField<4, 1, u32> alphaupdate;
  BitField<5, 1, u32> subtract;
  BitField<6, 1, u32> subtractAlpha;
  BitField<7, 1, u32> usedualsrc;
  BitField<8, 3, BlendMode::BlendFactor> dstfactor;
  BitField<11, 3, BlendMode::BlendFactor> srcfactor;
  BitField<14, 3, BlendMode::BlendFactor> dstfactoralpha;
  BitField<17, 3, BlendMode::BlendFactor> srcfactoralpha;
  BitField<20, 4, BlendMode::LogicOp> logicmode;

  u32 hex;
};

namespace RenderState
{
RasterizationState GetNoCullRasterizationState();
DepthState GetNoDepthTestingDepthStencilState();
BlendingState GetNoBlendingBlendState();
}
