// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"

#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"

enum class APIType;
enum class TexInputForm : u8;
enum class TexGenType : u8;
enum class SourceRow : u8;
enum class TexSize : u8;
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

// Currently optimized to 28 bytes (with 8 bits spare)
// The smaller (and less redundant) this is, the better.
// Though, it probably does like to be somewhat aligned.
struct alignas(4) vertex_shader_uid_data
{
  u32 NumValues() const { return sizeof(vertex_shader_uid_data); }
  u32 components : 14;
  u32 position_has_3_elems : 1;
  u32 dualTexTrans_enabled : 1;
  u32 numTexGens : 4;  // if more bits are needed, this could be eliminated by somehow marking the
                       // first unused texGen as empty.
  u32 numColorChans : 2;  // Output color channels.
  VSExpand vs_expand : 2;

  u32 pad : 8;

  // texInfo is optimized to fit all per-texgen config into just 16-bits.
  // But it did require a union
  struct
  {
    u8 texcoord_elem_count : 2;
    TexInputForm inputform : 1;
    SourceRow sourcerow : 4;
    bool is_regular_texgen : 1;  // union tag
    union
    {
      struct
      {
        u8 postmtx_index : 6;
        u8 postmtx_normalize : 1;
        TexSize projection : 1;
      } regular;
      struct
      {
        TexGenType texgentype : 2;
        // these are only used by EmbossMap texgen.
        u8 emboss_sourceshift : 3;
        u8 emboss_lightshift : 3;
      } other;
    };
  } texGenInfo[8];

  static_assert(sizeof(texGenInfo[0]) == 2, "texGenInfo should be 2 bytes per texgen");

  LightingUidData lighting;
};

// We do need to make sure lighting is correctly aligned
static_assert(offsetof(vertex_shader_uid_data, lighting) % alignof(LightingUidData) == 0);
static_assert(offsetof(vertex_shader_uid_data, texGenInfo) % alignof(u32) == 0);
static_assert(sizeof(vertex_shader_uid_data) == 28, "vertex_shader_uid_data should be 28 bytes");

using VertexShaderUid = ShaderUid<vertex_shader_uid_data>;

struct CustomVertexContents
{
  std::string_view shader = "";
  std::string_view uniforms = "";
};

VertexShaderUid GetVertexShaderUid();
ShaderCode GenerateVertexShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                    const vertex_shader_uid_data* uid_data,
                                    CustomVertexContents custom_contents);
void WriteVertexBody(APIType api_type, const ShaderHostConfig& host_config,
                     const vertex_shader_uid_data* uid_data, ShaderCode& out);
