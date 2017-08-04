// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/TextureDecoder.h"

enum class APIType;

#pragma pack(1)
struct convertion_shader_uid_data
{
  u32 NumValues() const { return sizeof(convertion_shader_uid_data); }
  EFBCopyFormat dst_format;

  u32 efb_has_alpha : 1;
  u32 is_depth_copy : 1;
  u32 is_intensity : 1;
  u32 scale_by_half : 1;
};
#pragma pack()

using TextureConverterShaderUid = ShaderUid<convertion_shader_uid_data>;

ShaderCode GenerateTextureConverterShaderCode(APIType api_type,
                                              const convertion_shader_uid_data* uid_data);

TextureConverterShaderUid GetTextureConverterShaderUid(EFBCopyFormat dst_format, bool is_depth_copy,
                                                       bool is_intensity, bool scale_by_half);
