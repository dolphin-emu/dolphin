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

  u32 _data1;

  BFVIEW_M(_data1, u32, 0, 23, components);
  BFVIEW_M(_data1, u32, 23, 4, numTexGens);
  BFVIEW_M(_data1, u32, 27, 2, numColorChans);
  BFVIEW_M(_data1, bool, 29, 1, dualTexTrans_enabled);

  struct
  {
    u16 _data2;

    BFVIEW_M(_data2, TexInputForm, 0, 2, inputform);
    BFVIEW_M(_data2, TexGenType, 2, 3, texgentype);
    BFVIEW_M(_data2, SourceRow, 5, 5, sourcerow);
    BFVIEW_M(_data2, u16, 10, 3, embosssourceshift);
    BFVIEW_M(_data2, u16, 13, 3, embosslightshift);
  } texMtxInfo[8];

  struct
  {
    u8 _data3;

    BFVIEW_M(_data3, u8, 0, 6, index);
    BFVIEW_M(_data3, bool, 6, 1, normalize);
  } postMtxInfo[8];

  LightingUidData lighting;

  // Stored separately to guarantee that the texMtxInfo struct is 8 bits wide
  u16 _data4;
  BFVIEWARRAY_M(_data4, bool, 0, 1, 16, texMtxInfo_n_projection);
};
#pragma pack()

using VertexShaderUid = ShaderUid<vertex_shader_uid_data>;

VertexShaderUid GetVertexShaderUid();
ShaderCode GenerateVertexShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                    const vertex_shader_uid_data* uid_data);
