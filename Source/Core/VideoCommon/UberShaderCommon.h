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

// bitfieldExtract generator for BitField types
template <typename T>
std::string BitfieldExtract(const std::string& source, T type)
{
  return StringFromFormat("bitfieldExtract(%s, %u, %u)", source.c_str(),
                          static_cast<u32>(type.StartBit()), static_cast<u32>(type.NumBits()));
}

}  // namespace UberShader
