// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/CommonTypes.h"

// all constant buffer attributes must be 16 bytes aligned, so this are the only allowed components:
using float4 = std::array<float, 4>;
using uint4 = std::array<u32, 4>;
using int4 = std::array<s32, 4>;

enum class DstBlendFactor : u32;
enum class SrcBlendFactor : u32;
enum class ZTexOp : u32;
enum class LogicOp : u32;

struct alignas(16) PixelShaderConstants
{
  std::array<int4, 4> colors;
  std::array<int4, 4> kcolors;
  int4 alpha;
  std::array<uint4, 8> texdims;
  std::array<int4, 2> zbias;
  std::array<int4, 2> indtexscale;
  std::array<int4, 6> indtexmtx;
  int4 fogcolor;
  int4 fogi;
  float4 fogf;
  std::array<float4, 3> fogrange;
  float4 zslope;
  std::array<float, 2> efbscale;  // .xy

  // Constants from here onwards are only used in ubershaders, other than pack2.
  u32 genmode;                  // .z
  u32 alphaTest;                // .w
  u32 fogParam3;                // .x
  u32 fogRangeBase;             // .y
  u32 dstalpha;                 // .z
  ZTexOp ztex_op;               // .w
  u32 late_ztest;               // .x (bool)
  u32 rgba6_format;             // .y (bool)
  u32 dither;                   // .z (bool)
  u32 bounding_box;             // .w (bool)
  std::array<uint4, 16> pack1;  // .xy - combiners, .z - tevind, .w - iref
  std::array<uint4, 8> pack2;   // .x - tevorder, .y - tevksel, .z/.w - SamplerState tm0/tm1
  std::array<int4, 32> konst;   // .rgba
  // The following are used in ubershaders when using shader_framebuffer_fetch blending
  u32 blend_enable;
  SrcBlendFactor blend_src_factor;
  SrcBlendFactor blend_src_factor_alpha;
  DstBlendFactor blend_dst_factor;
  DstBlendFactor blend_dst_factor_alpha;
  u32 blend_subtract;
  u32 blend_subtract_alpha;
  // For shader_framebuffer_fetch logic ops:
  u32 logic_op_enable;  // bool
  LogicOp logic_op_mode;
  // For custom shaders...
  u32 time_ms;
};

struct alignas(16) VertexShaderConstants
{
  u32 components;           // .x
  u32 xfmem_dualTexInfo;    // .y
  u32 xfmem_numColorChans;  // .z
  u32 missing_color_hex;    // .w, used for change detection but not directly by shaders
  float4 missing_color_value;

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
  float4 pixelpositioncorrection;
  std::array<float, 2> viewport;  // .xy
  std::array<float, 2> pad2;      // .zw

  // .x - texMtxInfo, .y - postMtxInfo, [0..1].z = color, [0..1].w = alpha
  std::array<uint4, 8> xfmem_pack1;

  float4 cached_tangent;
  float4 cached_binormal;
  // For UberShader vertex loader
  u32 vertex_stride;
  std::array<u32, 3> vertex_offset_normals;
  u32 vertex_offset_position;
  u32 vertex_offset_posmtx;
  std::array<u32, 2> vertex_offset_colors;
  std::array<u32, 8> vertex_offset_texcoords;
};

enum class VSExpand : u32
{
  None = 0,
  Point,
  Line,
};

struct alignas(16) GeometryShaderConstants
{
  float4 stereoparams;
  float4 lineptparams;
  int4 texoffset;
  VSExpand vs_expand;  // Used by VS point/line expansion in ubershaders
  u32 pad[3];
};
