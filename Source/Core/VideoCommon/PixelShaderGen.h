// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"

enum class APIType;
enum class AlphaTestOp : u32;
enum class AlphaTestResult;
enum class CompareMode : u32;
enum class DstBlendFactor : u32;
enum class EmulatedZ : u32;
enum class FogProjection : u32;
enum class FogType : u32;
enum class KonstSel : u32;
enum class RasColorChan : u32;
enum class SrcBlendFactor : u32;
enum class ZTexOp : u32;
enum class LogicOp : u32;

#pragma pack(1)
struct pixel_shader_uid_data
{
  // TODO: Optimize field order for easy access!

  u32 num_values;  // TODO: Shouldn't be a u32
  u32 NumValues() const { return num_values; }

  u32 _data1;
  u32 _data2;
  u32 _data3;

  BFVIEW_IN(_data1, bool, 1, 0, useDstAlpha)
  BFVIEW_IN(_data1, bool, 1, 1, no_dual_src)
  BFVIEW_IN(_data1, AlphaTestResult, 2, 2, Pretest)
  BFVIEW_IN(_data1, bool, 1, 4, nIndirectStagesUsed, 4)  // 4x1 bit
  BFVIEW_IN(_data1, u32, 4, 8, genMode_numtexgens)
  BFVIEW_IN(_data1, u32, 4, 12, genMode_numtevstages)
  BFVIEW_IN(_data1, u32, 3, 16, genMode_numindstages)
  BFVIEW_IN(_data1, CompareMode, 3, 19, alpha_test_comp0)
  BFVIEW_IN(_data1, CompareMode, 3, 22, alpha_test_comp1)
  BFVIEW_IN(_data1, AlphaTestOp, 2, 25, alpha_test_logic)
  BFVIEW_IN(_data1, FogProjection, 1, 27, fog_proj)
  BFVIEW_IN(_data1, FogType, 3, 28, fog_fsel)
  BFVIEW_IN(_data1, bool, 1, 31, fog_RangeBaseEnabled)

  BFVIEW_IN(_data2, ZTexOp, 2, 0, ztex_op)
  BFVIEW_IN(_data2, bool, 1, 2, per_pixel_depth)
  BFVIEW_IN(_data2, EmulatedZ, 3, 3, ztest)
  BFVIEW_IN(_data2, bool, 1, 6, bounding_box)
  BFVIEW_IN(_data2, bool, 1, 7, zfreeze)
  BFVIEW_IN(_data2, u32, 2, 8, numColorChans)
  BFVIEW_IN(_data2, bool, 1, 10, rgba6_format)
  BFVIEW_IN(_data2, bool, 1, 11, dither)
  BFVIEW_IN(_data2, bool, 1, 12, uint_output)

  // Only used with shader_framebuffer_fetch blend
  BFVIEW_IN(_data2, bool, 1, 13, blend_enable)                      // shader_framebuffer_fetch
  BFVIEW_IN(_data2, SrcBlendFactor, 3, 14, blend_src_factor)        // shader_framebuffer_fetch
  BFVIEW_IN(_data2, SrcBlendFactor, 3, 17, blend_src_factor_alpha)  // shader_framebuffer_fetch
  BFVIEW_IN(_data2, DstBlendFactor, 3, 20, blend_dst_factor)        // shader_framebuffer_fetch
  BFVIEW_IN(_data2, DstBlendFactor, 3, 23, blend_dst_factor_alpha)  // shader_framebuffer_fetch
  BFVIEW_IN(_data2, bool, 1, 26, blend_subtract)                    // shader_framebuffer_fetch
  BFVIEW_IN(_data2, bool, 1, 27, blend_subtract_alpha)              // shader_framebuffer_fetch
  // 4 bits of padding

  BFVIEW_IN(_data3, bool, 1, 0, logic_op_enable)   // shader_framebuffer_fetch
  BFVIEW_IN(_data3, LogicOp, 4, 1, logic_op_mode)  // shader_framebuffer_fetch
  BFVIEW_IN(_data3, u32, 3, 5, tevindref_bi0)
  BFVIEW_IN(_data3, u32, 3, 8, tevindref_bc0)
  BFVIEW_IN(_data3, u32, 3, 11, tevindref_bi1)
  BFVIEW_IN(_data3, u32, 3, 14, tevindref_bc1)
  BFVIEW_IN(_data3, u32, 3, 17, tevindref_bi2)
  BFVIEW_IN(_data3, u32, 3, 20, tevindref_bc2)
  BFVIEW_IN(_data3, u32, 3, 23, tevindref_bi3)
  BFVIEW_IN(_data3, u32, 3, 26, tevindref_bc3)
  // 2 bits of padding

  // Stored separately to guarantee that the texMtxInfo struct is 8 bits wide
  BFVIEW_IN(_data4, TexSize, 1, 0, texMtxInfo_n_projection, 8)  // 8x1 bit

  void SetTevindrefValues(int index, u32 texcoord, u32 texmap)
  {
    if (index == 0)
    {
      tevindref_bc0() = texcoord;
      tevindref_bi0() = texmap;
    }
    else if (index == 1)
    {
      tevindref_bc1() = texcoord;
      tevindref_bi1() = texmap;
    }
    else if (index == 2)
    {
      tevindref_bc2() = texcoord;
      tevindref_bi2() = texmap;
    }
    else if (index == 3)
    {
      tevindref_bc3() = texcoord;
      tevindref_bi3() = texmap;
    }
  }

  u32 GetTevindirefCoord(int index) const
  {
    if (index == 0)
      return tevindref_bc0();
    else if (index == 1)
      return tevindref_bc1();
    else if (index == 2)
      return tevindref_bc2();
    else if (index == 3)
      return tevindref_bc3();
    return 0;
  }

  u32 GetTevindirefMap(int index) const
  {
    if (index == 0)
      return tevindref_bi0();
    else if (index == 1)
      return tevindref_bi1();
    else if (index == 2)
      return tevindref_bi2();
    else if (index == 3)
      return tevindref_bi3();
    return 0;
  }

  struct
  {
    // TODO: Can save a lot space by removing the padding bits
    u64 _data1;
    u64 _data2;

    // Is there an int24_t?  C23's _BitInt?
    BFVIEW_IN(_data1, u32, 24, 0, cc)
    BFVIEW_IN(_data1, u32, 24, 24, ac)  // tswap and rswap are left blank
                                        // (decoded into the swap fields below)
    BFVIEW_IN(_data1, u32, 3, 48, tevorders_texmap)
    BFVIEW_IN(_data1, u32, 3, 51, tevorders_texcoord)
    BFVIEW_IN(_data1, bool, 1, 54, tevorders_enable)
    BFVIEW_IN(_data1, RasColorChan, 3, 55, tevorders_colorchan)
    // 6 bits of padding

    // TODO: We could save space by storing the 4 swap tables elsewhere and only storing references
    // to which table is used (the tswap and rswap fields), instead of duplicating them here
    BFVIEW_IN(_data2, u32, 21, 0, tevind)
    BFVIEW_IN(_data2, ColorChannel, 2, 21, ras_swap_r)
    BFVIEW_IN(_data2, ColorChannel, 2, 23, ras_swap_g)
    BFVIEW_IN(_data2, ColorChannel, 2, 25, ras_swap_b)
    BFVIEW_IN(_data2, ColorChannel, 2, 27, ras_swap_a)
    BFVIEW_IN(_data2, ColorChannel, 2, 29, tex_swap_r)
    BFVIEW_IN(_data2, ColorChannel, 2, 31, tex_swap_g)
    BFVIEW_IN(_data2, ColorChannel, 2, 33, tex_swap_b)
    BFVIEW_IN(_data2, ColorChannel, 2, 35, tex_swap_a)
    BFVIEW_IN(_data2, KonstSel, 5, 37, tevksel_kc)
    BFVIEW_IN(_data2, KonstSel, 5, 42, tevksel_ka)
    // 17 bits of padding
  } stagehash[16];

  LightingUidData lighting;
  u8 _data4;
};
#pragma pack()

using PixelShaderUid = ShaderUid<pixel_shader_uid_data>;

ShaderCode GeneratePixelShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                   const pixel_shader_uid_data* uid_data);
void WritePixelShaderCommonHeader(ShaderCode& out, APIType api_type,
                                  const ShaderHostConfig& host_config, bool bounding_box);
void ClearUnusedPixelShaderUidBits(APIType api_type, const ShaderHostConfig& host_config,
                                   PixelShaderUid* uid);
PixelShaderUid GetPixelShaderUid();
