// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

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

  u32 efb_has_alpha : 1;
  u32 is_depth_copy : 1;
  u32 is_intensity : 1;
  u32 scale_by_half : 1;
  u32 copy_filter : 1;
};
#pragma pack()

using TCShaderUid = ShaderUid<UidData>;

ShaderCode GenerateVertexShader(APIType api_type);
ShaderCode GeneratePixelShader(APIType api_type, const UidData* uid_data);

TCShaderUid GetShaderUid(EFBCopyFormat dst_format, bool is_depth_copy, bool is_intensity,
                         bool scale_by_half, bool copy_filter);

}  // namespace TextureConversionShaderGen
