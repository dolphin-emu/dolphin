// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"

// all constant buffer attributes must be 16 bytes aligned, so this are the only allowed components:
using float4 = std::array<float, 4>;
using int4 = std::array<s32, 4>;

struct PixelShaderConstants
{
  std::array<int4, 4> colors;
  std::array<int4, 4> kcolors;
  int4 alpha;
  std::array<float4, 8> texdims;
  std::array<int4, 2> zbias;
  std::array<int4, 2> indtexscale;
  std::array<int4, 6> indtexmtx;
  int4 fogcolor;
  int4 fogi;
  float4 fogf;
  std::array<float4, 3> fogrange;
  float4 zslope;
  std::array<float, 2> efbscale;  // .xy
};

struct VertexShaderConstants
{
  std::array<float4, 6> posnormalmatrix;
  std::array<float4, 4> projection;
  std::array<int4, 4> materials;
  struct Light
  {
    int4 color;
    float4 cosatt;
    float4 distatt;
    float4 pos;
    float4 dir;
  };
  std::array<Light, 8> lights;
  std::array<float4, 24> texmatrices;
  std::array<float4, 64> transformmatrices;
  std::array<float4, 32> normalmatrices;
  std::array<float4, 64> posttransformmatrices;
  float4 pixelcentercorrection;
  std::array<float, 2> viewport;  // .xy
};

struct GeometryShaderConstants
{
  float4 lineptparams;
  int4 texoffset;
};
