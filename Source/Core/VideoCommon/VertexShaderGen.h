// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"

enum class APIType;
enum class TexInputForm : bool;
enum class TexGenType : u32;
enum class SourceRow : u32;

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
  u16 _data2;

  BFVIEW_IN(_data1, u32, 23, 0, components)
  BFVIEW_IN(_data1, u32, 4, 23, numTexGens)
  BFVIEW_IN(_data1, u32, 2, 27, numColorChans)
  BFVIEW_IN(_data1, bool, 1, 29, dualTexTrans_enabled)
  // 1 bit padding

  // Stored separately to guarantee that the texMtxInfo struct is 8 bits wide
  BFVIEW_IN(_data2, TexSize, 1, 0, texMtxInfo_n_projection, 16);

  struct
  {
    u16 hex;

    BFVIEW(TexInputForm, 1, 0, inputform)  // inputform used to be 2 bits wide, but documentation
    // 1 bit padding.                         in XFMemory.h suggests the second bit is unknown.
    BFVIEW(TexGenType, 3, 2, texgentype)
    BFVIEW(SourceRow, 5, 5, sourcerow)
    BFVIEW(u16, 3, 10, embosssourceshift)
    BFVIEW(u16, 3, 13, embosslightshift)
  } texMtxInfo[8];

  struct
  {
    u8 hex;

    BFVIEW(u8, 6, 0, index)
    BFVIEW(bool, 1, 6, normalize)
  } postMtxInfo[8];

  LightingUidData lighting;
};
#pragma pack()

using VertexShaderUid = ShaderUid<vertex_shader_uid_data>;

VertexShaderUid GetVertexShaderUid();
ShaderCode GenerateVertexShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                    const vertex_shader_uid_data* uid_data);
