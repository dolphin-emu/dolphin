// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"

// all constant buffer attributes must be 16 bytes aligned, so this are the only allowed components:
using float4 = std::array<float, 4>;
using uint4 = std::array<u32, 4>;
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
  std::array<float4, 2> fogf;
  float4 zslope;
  std::array<float, 2> efbscale;

  // Constants from here onwards are only used in ubershaders.
  u32 genmode;                  // .z
  u32 alphaTest;                // .w
  u32 fogParam3;                // .x
  u32 fogRangeBase;             // .y
  u32 dstalpha;                 // .z
  u32 ztex_op;                  // .w
  u32 late_ztest;               // .x (bool)
  u32 rgba6_format;             // .y (bool)
  u32 dither;                   // .z (bool)
  u32 bounding_box;             // .w (bool)
  std::array<uint4, 16> pack1;  // .xy - combiners, .z - tevind, .w - iref
  std::array<uint4, 8> pack2;   // .x - tevorder, .y - tevksel
  std::array<int4, 32> konst;   // .rgba
};

struct VertexShaderConstants
{
  u32 components;           // .x
  u32 xfmem_dualTexInfo;    // .y
  u32 xfmem_numColorChans;  // .z
  u32 pad1;                 // .w

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
  std::array<float, 2> pad2;      // .zw

  // .x - texMtxInfo, .y - postMtxInfo, [0..1].z = color, [0..1].w = alpha
  std::array<uint4, 8> xfmem_pack1;
};

struct GeometryShaderConstants
{
  float4 stereoparams;
  float4 lineptparams;
  int4 texoffset;
};
