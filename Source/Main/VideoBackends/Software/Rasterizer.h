// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

struct OutputVertexData;

namespace Rasterizer
{
void Init();

void DrawTriangleFrontFace(const OutputVertexData* v0, const OutputVertexData* v1,
                           const OutputVertexData* v2);

void SetTevReg(int reg, int comp, s16 color);

struct Slope
{
  float dfdx;
  float dfdy;
  float f0;

  float GetValue(float dx, float dy) const { return f0 + (dfdx * dx) + (dfdy * dy); }
};

struct RasterBlockPixel
{
  float InvW;
  float Uv[8][2];
};

struct RasterBlock
{
  RasterBlockPixel Pixel[2][2];
  s32 IndirectLod[4];
  bool IndirectLinear[4];
  s32 TextureLod[16];
  bool TextureLinear[16];
};
}
