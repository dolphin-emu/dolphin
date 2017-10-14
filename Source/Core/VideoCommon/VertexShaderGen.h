// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"

enum class APIType;

// TODO should be reordered
#define SHADER_POSITION_ATTRIB 0
#define SHADER_POSMTX_ATTRIB 1
#define SHADER_NORM0_ATTRIB 2
#define SHADER_NORM1_ATTRIB 3
#define SHADER_NORM2_ATTRIB 4
#define SHADER_COLOR0_ATTRIB 5
#define SHADER_COLOR1_ATTRIB 6

#define SHADER_TEXTURE0_ATTRIB 8
#define SHADER_TEXTURE1_ATTRIB 9
#define SHADER_TEXTURE2_ATTRIB 10
#define SHADER_TEXTURE3_ATTRIB 11
#define SHADER_TEXTURE4_ATTRIB 12
#define SHADER_TEXTURE5_ATTRIB 13
#define SHADER_TEXTURE6_ATTRIB 14
#define SHADER_TEXTURE7_ATTRIB 15

#pragma pack(1)

struct vertex_shader_uid_data
{
  u32 NumValues() const { return sizeof(vertex_shader_uid_data); }
  u32 components : 23;
  u32 numTexGens : 4;
  u32 numColorChans : 2;
  u32 dualTexTrans_enabled : 1;

  u32 texMtxInfo_n_projection : 16;  // Stored separately to guarantee that the texMtxInfo struct is
                                     // 8 bits wide
  u32 pad : 18;

  struct
  {
    u32 inputform : 2;
    u32 texgentype : 3;
    u32 sourcerow : 5;
    u32 embosssourceshift : 3;
    u32 embosslightshift : 3;
  } texMtxInfo[8];

  struct
  {
    u32 index : 6;
    u32 normalize : 1;
    u32 pad : 1;
  } postMtxInfo[8];

  LightingUidData lighting;
};
#pragma pack()

typedef ShaderUid<vertex_shader_uid_data> VertexShaderUid;

VertexShaderUid GetVertexShaderUid();
ShaderCode GenerateVertexShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                    const vertex_shader_uid_data* uid_data);
