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
enum class FogProjection : u32;
enum class FogType : u32;
enum class KonstSel : u32;
enum class RasColorChan : u32;
enum class SrcBlendFactor : u32;
enum class ZTexOp : u32;

#pragma pack(1)
struct pixel_shader_uid_data
{
  // TODO: Optimize field order for easy access!

  u32 num_values;  // TODO: Shouldn't be a u32
  u32 NumValues() const { return num_values; }

  u32 _data1;

  BFVIEW_M(_data1, u32, 0, 4, pad0);
  BFVIEW_M(_data1, u32, 4, 1, useDstAlpha);
  BFVIEW_M(_data1, AlphaTestResult, 5, 2, Pretest);
  BFVIEW_M(_data1, u32, 7, 4, nIndirectStagesUsed);
  BFVIEW_M(_data1, u32, 11, 4, genMode_numtexgens);
  BFVIEW_M(_data1, u32, 15, 4, genMode_numtevstages);
  BFVIEW_M(_data1, u32, 19, 3, genMode_numindstages);
  BFVIEW_M(_data1, CompareMode, 22, 3, alpha_test_comp0);
  BFVIEW_M(_data1, CompareMode, 25, 3, alpha_test_comp1);
  BFVIEW_M(_data1, AlphaTestOp, 28, 2, alpha_test_logic);
  BFVIEW_M(_data1, u32, 30, 1, alpha_test_use_zcomploc_hack);
  BFVIEW_M(_data1, FogProjection, 31, 1, fog_proj);

  u32 _data2;

  BFVIEW_M(_data2, FogType, 0, 3, fog_fsel);
  BFVIEW_M(_data2, bool, 3, 1, fog_RangeBaseEnabled);
  BFVIEW_M(_data2, ZTexOp, 4, 2, ztex_op);
  BFVIEW_M(_data2, bool, 6, 1, per_pixel_depth);
  BFVIEW_M(_data2, bool, 7, 1, forced_early_z);
  BFVIEW_M(_data2, bool, 8, 1, early_ztest);
  BFVIEW_M(_data2, bool, 9, 1, late_ztest);
  BFVIEW_M(_data2, bool, 10, 1, bounding_box);
  BFVIEW_M(_data2, bool, 11, 1, zfreeze);
  BFVIEW_M(_data2, u32, 12, 2, numColorChans);
  BFVIEW_M(_data2, bool, 14, 1, rgba6_format);
  BFVIEW_M(_data2, bool, 15, 1, dither);
  BFVIEW_M(_data2, bool, 16, 1, uint_output);

  // shader_framebuffer_fetch blend
  BFVIEW_M(_data2, bool, 17, 1, blend_enable);
  BFVIEW_M(_data2, SrcBlendFactor, 18, 3, blend_src_factor);
  BFVIEW_M(_data2, SrcBlendFactor, 21, 3, blend_src_factor_alpha);
  BFVIEW_M(_data2, DstBlendFactor, 24, 3, blend_dst_factor);
  BFVIEW_M(_data2, DstBlendFactor, 27, 3, blend_dst_factor_alpha);
  BFVIEW_M(_data2, bool, 30, 1, blend_subtract);
  BFVIEW_M(_data2, bool, 31, 1, blend_subtract_alpha);

  u32 _data3;

  // shader_framebuffer_fetch logic ops
  BFVIEW_M(_data3, bool, 0, 1, logic_op_enable);
  BFVIEW_M(_data3, LogicOp, 1, 4, logic_op_mode);

  BFVIEW_M(_data3, u32, 5, 3, tevindref_bi0);
  BFVIEW_M(_data3, u32, 8, 3, tevindref_bc0);
  BFVIEW_M(_data3, u32, 11, 3, tevindref_bi1);
  BFVIEW_M(_data3, u32, 14, 3, tevindref_bc1);
  BFVIEW_M(_data3, u32, 17, 3, tevindref_bi2);
  BFVIEW_M(_data3, u32, 20, 3, tevindref_bc2);
  BFVIEW_M(_data3, u32, 23, 3, tevindref_bi3);
  BFVIEW_M(_data3, u32, 26, 3, tevindref_bc3);

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
    u64 _data4;

    BFVIEW_M(_data4, u64, 0, 24, cc);
    BFVIEW_M(_data4, u64, 24, 24, ac);  // tswap and rswap are left blank
                                        // (encoded into the tevksel fields below)
    BFVIEW_M(_data4, u64, 48, 3, tevorders_texmap);
    BFVIEW_M(_data4, u64, 51, 3, tevorders_texcoord);
    BFVIEW_M(_data4, u64, 54, 1, tevorders_enable);
    BFVIEW_M(_data4, RasColorChan, 55, 3, tevorders_colorchan);
    BFVIEW_M(_data4, u64, 58, 6, pad1);

    // TODO: Clean up the swapXY mess
    u32 _data5;

    BFVIEW_M(_data5, u32, 0, 21, tevind);
    BFVIEW_M(_data5, u32, 21, 2, tevksel_swap1a);
    BFVIEW_M(_data5, u32, 23, 2, tevksel_swap2a);
    BFVIEW_M(_data5, u32, 25, 2, tevksel_swap1b);
    BFVIEW_M(_data5, u32, 27, 2, tevksel_swap2b);
    BFVIEW_M(_data5, u32, 29, 3, pad2);

    u32 _data6;

    BFVIEW_M(_data6, u32, 0, 2, tevksel_swap1c);
    BFVIEW_M(_data6, u32, 2, 2, tevksel_swap2c);
    BFVIEW_M(_data6, u32, 4, 2, tevksel_swap1d);
    BFVIEW_M(_data6, u32, 6, 2, tevksel_swap2d);
    BFVIEW_M(_data6, KonstSel, 8, 5, tevksel_kc);
    BFVIEW_M(_data6, KonstSel, 13, 5, tevksel_ka);
    BFVIEW_M(_data6, u32, 18, 14, pad3);
  } stagehash[16];

  LightingUidData lighting;
  // Stored separately to guarantee that the texMtxInfo struct is 8 bits wide
  u8 _data7;
  BFVIEWARRAY_M(_data7, bool, 0, 1, 8, texMtxInfo_n_projection);
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
