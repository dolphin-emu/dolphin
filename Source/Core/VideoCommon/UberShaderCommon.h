// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/TypeUtils.h"

class ShaderCode;
enum class APIType;
union ShaderHostConfig;

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
}  // namespace UberShader
