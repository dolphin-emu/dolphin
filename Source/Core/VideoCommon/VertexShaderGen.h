// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"

enum class APIType;
enum class TexInputForm : u16;
enum class TexGenType : u16;
enum class SourceRow : u16;

// TODO should be reordered
enum : int
{
  SHADER_POSITION_ATTRIB = 0,
  SHADER_POSMTX_ATTRIB = 1,
  SHADER_NORMAL_ATTRIB = 2,
  SHADER_TANGENT_ATTRIB = 3,
  SHADER_BINORMAL_ATTRIB = 4,
  SHADER_COLOR0_ATTRIB = 5,
  SHADER_COLOR1_ATTRIB = 6,

  SHADER_TEXTURE0_ATTRIB = 8,
  SHADER_TEXTURE1_ATTRIB = 9,
  SHADER_TEXTURE2_ATTRIB = 10,
  SHADER_TEXTURE3_ATTRIB = 11,
  SHADER_TEXTURE4_ATTRIB = 12,
  SHADER_TEXTURE5_ATTRIB = 13,
  SHADER_TEXTURE6_ATTRIB = 14,
  SHADER_TEXTURE7_ATTRIB = 15
};

#pragma pack(1)

struct vertex_shader_uid_data
{
  u32 NumValues() const { return sizeof(vertex_shader_uid_data); }

  union
  {
    BitField<0, 23, u32> components;
    BitField<23, 4, u32> numTexGens;
    BitField<27, 2, u32> numColorChans;
    BitField<29, 1, bool, u32> dualTexTrans_enabled;
  };

  union
  {
    BitField<0, 2, TexInputForm, u16> inputform;
    BitField<2, 3, TexGenType, u16> texgentype;
    BitField<5, 5, SourceRow, u16> sourcerow;
    BitField<10, 3, u16> embosssourceshift;
    BitField<13, 3, u16> embosslightshift;
  } texMtxInfo[8];

  union
  {
    BitField<0, 6, u8> index;
    BitField<6, 1, bool, u8> normalize;
  } postMtxInfo[8];

  LightingUidData lighting;
  // Stored separately to guarantee that the texMtxInfo struct is 8 bits wide
  BitFieldArray<0, 1, 16, bool, u16> texMtxInfo_n_projection;
};
#pragma pack()

using VertexShaderUid = ShaderUid<vertex_shader_uid_data>;

VertexShaderUid GetVertexShaderUid();
ShaderCode GenerateVertexShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                    const vertex_shader_uid_data* uid_data);
