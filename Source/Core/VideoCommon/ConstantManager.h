// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// all constant buffer attributes must be 16 bytes aligned, so this are the only allowed components:
typedef float float4[4];
typedef u32 uint4[4];
typedef s32 int4[4];

struct PixelShaderConstants
{
  int4 colors[4];
  int4 kcolors[4];
  int4 alpha;
  float4 texdims[8];
  int4 zbias[2];
  int4 indtexscale[2];
  int4 indtexmtx[6];
  int4 fogcolor;
  int4 fogi;
  float4 fogf[2];
  float4 zslope;
  float efbscale[2];

  // Constants from here onwards are only used in ubershaders.
  u32 genmode;       // .z
  u32 alphaTest;     // .w
  u32 fogParam3;     // .x
  u32 fogRangeBase;  // .y
  u32 dstalpha;      // .z
  u32 ztex_op;       // .w
  u32 late_ztest;    // .x (bool)
  u32 rgba6_format;  // .y (bool)
  u32 dither;        // .z (bool)
  u32 bounding_box;  // .w (bool)
  uint4 pack1[16];   // .xy - combiners, .z - tevind, .w - iref
  uint4 pack2[8];    // .x - tevorder, .y - tevksel
  int4 konst[32];    // .rgba
};

struct VertexShaderConstants
{
  u32 components;           // .x
  u32 xfmem_dualTexInfo;    // .y
  u32 xfmem_numColorChans;  // .z
  u32 pad1;                 // .w

  float4 posnormalmatrix[6];
  float4 projection[4];
  int4 materials[4];
  struct Light
  {
    int4 color;
    float4 cosatt;
    float4 distatt;
    float4 pos;
    float4 dir;
  } lights[8];
  float4 texmatrices[24];
  float4 transformmatrices[64];
  float4 normalmatrices[32];
  float4 posttransformmatrices[64];
  float4 pixelcentercorrection;
  float viewport[2];  // .xy
  float pad2[2];      // .zw

  uint4 xfmem_pack1[8];  // .x - texMtxInfo, .y - postMtxInfo, [0..1].z = color, [0..1].w = alpha
};

struct GeometryShaderConstants
{
  float4 stereoparams;
  float4 lineptparams;
  int4 texoffset;
};
