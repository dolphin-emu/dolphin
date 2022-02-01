// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fmt/format.h>
#include <functional>

#include "Common/CommonTypes.h"

#include "VideoCommon/RenderState.h"
#include "VideoCommon/ShaderGenCommon.h"

enum class APIType;

#pragma pack(1)
struct geometry_shader_uid_data
{
  u32 NumValues() const { return sizeof(geometry_shader_uid_data); }
  bool IsPassthrough() const;

  u32 numTexGens : 4;
  u32 primitive_type : 2;
};
#pragma pack()

using GeometryShaderUid = ShaderUid<geometry_shader_uid_data>;

ShaderCode GenerateGeometryShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                      const geometry_shader_uid_data* uid_data);
GeometryShaderUid GetGeometryShaderUid(PrimitiveType primitive_type);
void EnumerateGeometryShaderUids(const std::function<void(const GeometryShaderUid&)>& callback);

template <>
struct fmt::formatter<geometry_shader_uid_data>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const geometry_shader_uid_data& uid, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "passthrough: {}, {} tex gens, primitive type {}",
                          uid.IsPassthrough(), uid.numTexGens, uid.primitive_type);
  }
};
