// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include "VideoCommon/PixelShaderGen.h"

namespace UberShader
{
#pragma pack(1)
struct vertex_ubershader_uid_data
{
  u32 num_texgens : 4;

  u32 NumValues() const { return sizeof(vertex_ubershader_uid_data); }
};
#pragma pack()

using VertexShaderUid = ShaderUid<vertex_ubershader_uid_data>;

VertexShaderUid GetVertexShaderUid();

ShaderCode GenVertexShader(APIType api_type, const ShaderHostConfig& host_config,
                           const vertex_ubershader_uid_data* uid_data);
void EnumerateVertexShaderUids(const std::function<void(const VertexShaderUid&)>& callback);
}  // namespace UberShader

template <>
struct fmt::formatter<UberShader::vertex_ubershader_uid_data>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const UberShader::vertex_ubershader_uid_data& uid, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Vertex UberShader for {} texgens", uid.num_texgens);
  }
};
