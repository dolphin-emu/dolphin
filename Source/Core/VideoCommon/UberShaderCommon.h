// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VideoCommon.h"

namespace UberShader
{
// Common functions across all ubershaders
void WriteUberShaderCommonHeader(ShaderCode& out, APIType api_type,
                                 const ShaderHostConfig& host_config);

// Vertex lighting
void WriteLightingFunction(ShaderCode& out);
void WriteVertexLighting(ShaderCode& out, APIType api_type, const char* world_pos_var,
                         const char* normal_var, const char* in_color_0_var,
                         const char* in_color_1_var, const char* out_color_0_var,
                         const char* out_color_1_var);

// bitfieldExtract generator for BitField types
template <typename T>
std::string BitfieldExtract(const std::string& source, T type)
{
  return StringFromFormat("bitfieldExtract(%s, %u, %u)", source.c_str(),
                          static_cast<u32>(type.StartBit()), static_cast<u32>(type.NumBits()));
}
}  // namespace UberShader
