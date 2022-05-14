// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fmt/format.h>
#include <string>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/TextureDecoder.h"

enum class APIType;

namespace TextureConversionShaderGen
{
#pragma pack(1)
struct UidData
{
  u32 NumValues() const { return sizeof(UidData); }
  EFBCopyFormat dst_format;
  union
  {
    BitField<0, 1, bool, u32> efb_has_alpha;
    BitField<1, 1, bool, u32> is_depth_copy;
    BitField<2, 1, bool, u32> is_intensity;
    BitField<3, 1, bool, u32> scale_by_half;
    BitField<4, 1, bool, u32> copy_filter;
  };
};
#pragma pack()

using TCShaderUid = ShaderUid<UidData>;

ShaderCode GenerateVertexShader(APIType api_type);
ShaderCode GeneratePixelShader(APIType api_type, const UidData* uid_data);

TCShaderUid GetShaderUid(EFBCopyFormat dst_format, bool is_depth_copy, bool is_intensity,
                         bool scale_by_half, bool copy_filter);

}  // namespace TextureConversionShaderGen

template <>
struct fmt::formatter<TextureConversionShaderGen::UidData>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TextureConversionShaderGen::UidData& uid, FormatContext& ctx) const
  {
    std::string dst_format;
    if (uid.dst_format == EFBCopyFormat::XFB)
      dst_format = "XFB";
    else
      dst_format = fmt::to_string(uid.dst_format);
    return fmt::format_to(ctx.out(),
                          "dst_format: {}, efb_has_alpha: {}, is_depth_copy: {}, is_intensity: {}, "
                          "scale_by_half: {}, copy_filter: {}",
                          dst_format, uid.efb_has_alpha, uid.is_depth_copy, uid.is_intensity,
                          uid.scale_by_half, uid.copy_filter);
  }
};
