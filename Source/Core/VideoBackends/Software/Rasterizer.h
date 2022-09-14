// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

struct OutputVertexData;

namespace Rasterizer
{
void Init();
void ScissorChanged();

void UpdateZSlope(const OutputVertexData* v0, const OutputVertexData* v1,
                  const OutputVertexData* v2, s32 x_off, s32 y_off);
void DrawTriangleFrontFace(const OutputVertexData* v0, const OutputVertexData* v1,
                           const OutputVertexData* v2);

void SetTevKonstColors();

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
}  // namespace Rasterizer
