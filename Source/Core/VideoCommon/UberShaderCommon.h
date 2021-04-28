// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/TypeUtils.h"

#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VideoCommon.h"

namespace UberShader
{
// Common functions across all ubershaders
void WriteUberShaderCommonHeader(ShaderCode& out, APIType api_type,
                                 const ShaderHostConfig& host_config);

// Vertex lighting
void WriteLightingFunction(ShaderCode& out);
void WriteVertexLighting(ShaderCode& out, APIType api_type, std::string_view world_pos_var,
                         std::string_view normal_var, std::string_view in_color_0_var,
                         std::string_view in_color_1_var, std::string_view out_color_0_var,
                         std::string_view out_color_1_var);

// bitfieldExtract generator for BitField types
template <auto ptr_to_bitfield_member>
std::string BitfieldExtract(std::string_view source)
{
  using BitFieldT = Common::MemberType<ptr_to_bitfield_member>;
  return fmt::format("bitfieldExtract({}, {}, {})", source, static_cast<u32>(BitFieldT::StartBit()),
                     static_cast<u32>(BitFieldT::NumBits()));
}

template <auto last_member, typename = decltype(last_member)>
void WriteSwitch(ShaderCode& out, APIType ApiType, std::string_view variable,
                 const Common::EnumMap<std::string_view, last_member>& values, int indent,
                 bool break_)
{
  const bool make_switch = (ApiType == APIType::D3D);

  // The second template argument is needed to avoid compile errors from ambiguity with multiple
  // enums with the same number of members in GCC prior to 8.  See https://godbolt.org/z/xcKaW1seW
  // and https://godbolt.org/z/hz7Yqq1P5
  using enum_type = decltype(last_member);

  // {:{}} is used to indent by formatting an empty string with a variable width
  if (make_switch)
  {
    out.Write("{:{}}switch ({}) {{\n", "", indent, variable);
    for (u32 i = 0; i <= static_cast<u32>(last_member); i++)
    {
      const enum_type key = static_cast<enum_type>(i);

      // Assumes existence of an EnumFormatter
      out.Write("{:{}}case {:s}:\n", "", indent, key);
      // Note that this indentation behaves poorly for multi-line code
      if (!values[key].empty())
        out.Write("{:{}}  {}\n", "", indent, values[key]);
      if (break_)
        out.Write("{:{}}  break;\n", "", indent);
    }
    out.Write("{:{}}}}\n", "", indent);
  }
  else
  {
    // Generate a tree of if statements recursively
    // std::function must be used because auto won't capture before initialization and thus can't be
    // used recursively
    std::function<void(u32, u32, u32)> BuildTree = [&](u32 cur_indent, u32 low, u32 high) {
      // Each generated statement is for low <= x < high
      if (high == low + 1)
      {
        // Down to 1 case (low <= x < low + 1 means x == low)
        const enum_type key = static_cast<enum_type>(low);
        // Note that this indentation behaves poorly for multi-line code
        out.Write("{:{}}{}  // {}\n", "", cur_indent, values[key], key);
      }
      else
      {
        u32 mid = low + ((high - low) / 2);
        out.Write("{:{}}if ({} < {}u) {{\n", "", cur_indent, variable, mid);
        BuildTree(cur_indent + 2, low, mid);
        out.Write("{:{}}}} else {{\n", "", cur_indent);
        BuildTree(cur_indent + 2, mid, high);
        out.Write("{:{}}}}\n", "", cur_indent);
      }
    };
    BuildTree(indent, 0, static_cast<u32>(last_member) + 1);
  }
}
}  // namespace UberShader
