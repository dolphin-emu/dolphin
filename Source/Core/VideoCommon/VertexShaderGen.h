// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"

#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"

enum class APIType;
enum class TexInputForm : u32;
enum class TexGenType : u32;
enum class SourceRow : u32;
enum class VSExpand : u32;

// TODO should be reordered
enum class ShaderAttrib : u32
{
  Position = 0,
  PositionMatrix = 1,
  Normal = 2,
  Tangent = 3,
  Binormal = 4,
  Color0 = 5,
  Color1 = 6,

  TexCoord0 = 8,
  TexCoord1 = 9,
  TexCoord2 = 10,
  TexCoord3 = 11,
  TexCoord4 = 12,
  TexCoord5 = 13,
  TexCoord6 = 14,
  TexCoord7 = 15
};
template <>
struct fmt::formatter<ShaderAttrib> : EnumFormatter<ShaderAttrib::TexCoord7>
{
  static constexpr array_type names = {
      "Position",    "Position Matrix", "Normal",      "Tangent",     "Binormal",    "Color 0",
      "Color 1",     nullptr,           "Tex Coord 0", "Tex Coord 1", "Tex Coord 2", "Tex Coord 3",
      "Tex Coord 4", "Tex Coord 5",     "Tex Coord 6", "Tex Coord 7"};
  constexpr formatter() : EnumFormatter(names) {}
};
// Intended for offsetting from Color0/TexCoord0
constexpr ShaderAttrib operator+(ShaderAttrib attrib, int offset)
{
  return static_cast<ShaderAttrib>(static_cast<u8>(attrib) + offset);
}

#pragma pack(1)

struct vertex_shader_uid_data
{
  u32 NumValues() const { return sizeof(vertex_shader_uid_data); }
  u32 components : 23;
  u32 numTexGens : 4;
  u32 numColorChans : 2;
  u32 dualTexTrans_enabled : 1;
  VSExpand vs_expand : 2;
  u32 position_has_3_elems : 1;

  u16 texcoord_elem_count;      // 2 bits per texcoord input
  u16 texMtxInfo_n_projection;  // Stored separately to guarantee that the texMtxInfo struct is
                                // 8 bits wide

  struct
  {
    TexInputForm inputform : 2;
    TexGenType texgentype : 3;
    SourceRow sourcerow : 5;
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

using VertexShaderUid = ShaderUid<vertex_shader_uid_data>;

VertexShaderUid GetVertexShaderUid();
ShaderCode GenerateVertexShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                    const vertex_shader_uid_data* uid_data);
