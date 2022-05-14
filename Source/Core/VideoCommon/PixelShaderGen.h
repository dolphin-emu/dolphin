// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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

  union
  {
    BitField<0, 4, u32> pad0;
    BitField<4, 1, u32> useDstAlpha;
    BitField<5, 2, AlphaTestResult, u32> Pretest;
    BitField<7, 4, u32> nIndirectStagesUsed;
    BitField<11, 4, u32> genMode_numtexgens;
    BitField<15, 4, u32> genMode_numtevstages;
    BitField<19, 3, u32> genMode_numindstages;
    BitField<22, 3, CompareMode, u32> alpha_test_comp0;
    BitField<25, 3, CompareMode, u32> alpha_test_comp1;
    BitField<28, 2, AlphaTestOp, u32> alpha_test_logic;
    BitField<30, 1, u32> alpha_test_use_zcomploc_hack;
    BitField<31, 1, FogProjection, u32> fog_proj;
  };
  union
  {
    BitField<0, 3, FogType, u32> fog_fsel;
    BitField<3, 1, bool, u32> fog_RangeBaseEnabled;
    BitField<4, 2, ZTexOp, u32> ztex_op;
    BitField<6, 1, bool, u32> per_pixel_depth;
    BitField<7, 1, bool, u32> forced_early_z;
    BitField<8, 1, bool, u32> early_ztest;
    BitField<9, 1, bool, u32> late_ztest;
    BitField<10, 1, bool, u32> bounding_box;
    BitField<11, 1, bool, u32> zfreeze;
    BitField<12, 2, u32> numColorChans;
    BitField<14, 1, bool, u32> rgba6_format;
    BitField<15, 1, bool, u32> dither;
    BitField<16, 1, bool, u32> uint_output;
    BitField<17, 1, bool, u32> blend_enable;                      // shader_framebuffer_fetch blend
    BitField<18, 3, SrcBlendFactor, u32> blend_src_factor;        // shader_framebuffer_fetch blend
    BitField<21, 3, SrcBlendFactor, u32> blend_src_factor_alpha;  // shader_framebuffer_fetch blend
    BitField<24, 3, DstBlendFactor, u32> blend_dst_factor;        // shader_framebuffer_fetch blend
    BitField<27, 3, DstBlendFactor, u32> blend_dst_factor_alpha;  // shader_framebuffer_fetch blend
    BitField<30, 1, bool, u32> blend_subtract;                    // shader_framebuffer_fetch blend
    BitField<31, 1, bool, u32> blend_subtract_alpha;              // shader_framebuffer_fetch blend
  };
  union
  {
    BitField<0, 1, bool, u32> logic_op_enable;  // shader_framebuffer_fetch logic ops
    BitField<1, 4, u32> logic_op_mode;          // shader_framebuffer_fetch logic ops
    BitField<5, 3, u32> tevindref_bi0;
    BitField<8, 3, u32> tevindref_bc0;
    BitField<11, 3, u32> tevindref_bi1;
    BitField<14, 3, u32> tevindref_bc1;
    BitField<17, 3, u32> tevindref_bi2;
    BitField<20, 3, u32> tevindref_bc2;
    BitField<23, 3, u32> tevindref_bi3;
    BitField<26, 3, u32> tevindref_bc3;
  };

  void SetTevindrefValues(int index, u32 texcoord, u32 texmap)
  {
    if (index == 0)
    {
      tevindref_bc0 = texcoord;
      tevindref_bi0 = texmap;
    }
    else if (index == 1)
    {
      tevindref_bc1 = texcoord;
      tevindref_bi1 = texmap;
    }
    else if (index == 2)
    {
      tevindref_bc2 = texcoord;
      tevindref_bi2 = texmap;
    }
    else if (index == 3)
    {
      tevindref_bc3 = texcoord;
      tevindref_bi3 = texmap;
    }
  }

  u32 GetTevindirefCoord(int index) const
  {
    if (index == 0)
      return tevindref_bc0;
    else if (index == 1)
      return tevindref_bc1;
    else if (index == 2)
      return tevindref_bc2;
    else if (index == 3)
      return tevindref_bc3;
    return 0;
  }

  u32 GetTevindirefMap(int index) const
  {
    if (index == 0)
      return tevindref_bi0;
    else if (index == 1)
      return tevindref_bi1;
    else if (index == 2)
      return tevindref_bi2;
    else if (index == 3)
      return tevindref_bi3;
    return 0;
  }

  struct
  {
    // TODO: Can save a lot space by removing the padding bits
    union
    {
      BitField<0, 24, u64> cc;
      BitField<24, 24, u64> ac;  // tswap and rswap are left blank
                                 // (encoded into the tevksel fields below)
      BitField<48, 3, u64> tevorders_texmap;
      BitField<51, 3, u64> tevorders_texcoord;
      BitField<54, 1, u64> tevorders_enable;
      BitField<55, 3, RasColorChan, u64> tevorders_colorchan;
      BitField<58, 6, u64> pad1;
    };

    // TODO: Clean up the swapXY mess
    union
    {
      BitField<0, 21, u32> tevind;
      BitField<21, 2, u32> tevksel_swap1a;
      BitField<23, 2, u32> tevksel_swap2a;
      BitField<25, 2, u32> tevksel_swap1b;
      BitField<27, 2, u32> tevksel_swap2b;
      BitField<29, 3, u32> pad2;
    };
    union
    {
      BitField<0, 2, u32> tevksel_swap1c;
      BitField<2, 2, u32> tevksel_swap2c;
      BitField<4, 2, u32> tevksel_swap1d;
      BitField<6, 2, u32> tevksel_swap2d;
      BitField<8, 5, KonstSel, u32> tevksel_kc;
      BitField<13, 5, KonstSel, u32> tevksel_ka;
      BitField<18, 14, u32> pad3;
    };
  } stagehash[16];

  LightingUidData lighting;
  // Stored separately to guarantee that the texMtxInfo struct is 8 bits wide
  BitFieldArray<0, 1, 8, bool, u8> texMtxInfo_n_projection;
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
