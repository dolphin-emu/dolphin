// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>

#include "Common/CommonTypes.h"

enum class APIType;
enum class TextureFormat;
enum class EFBCopyFormat;
enum class TLUTFormat;
struct EFBCopyParams;

namespace TextureConversionShader
{
u16 GetEncodedSampleCount(EFBCopyFormat format);

const char* GenerateEncodingShader(const EFBCopyParams& params, APIType ApiType);

// View format of the input data to the texture decoding shader.
enum BufferFormat
{
  BUFFER_FORMAT_R8_UINT,
  BUFFER_FORMAT_R16_UINT,
  BUFFER_FORMAT_R32G32_UINT,
  BUFFER_FORMAT_COUNT
};

// Information required to compile and dispatch a texture decoding shader.
struct DecodingShaderInfo
{
  BufferFormat buffer_format;
  u32 palette_size;
  u32 group_size_x;
  u32 group_size_y;
  bool group_flatten;
  const char* shader_body;
};

// Obtain shader information for the specified texture format.
// If this format does not have a shader written for it, returns nullptr.
const DecodingShaderInfo* GetDecodingShaderInfo(TextureFormat format);

// Determine how many bytes there are in each element of the texel buffer.
// Needed for alignment and stride calculations.
u32 GetBytesPerBufferElement(BufferFormat buffer_format);

// Determine how many thread groups should be dispatched for an image of the specified width/height.
// First is the number of X groups, second is the number of Y groups, Z is always one.
std::pair<u32, u32> GetDispatchCount(const DecodingShaderInfo* info, u32 width, u32 height);

// Returns the GLSL string containing the texture decoding shader for the specified format.
std::string GenerateDecodingShader(TextureFormat format, TLUTFormat palette_format,
                                   APIType api_type);

}  // namespace TextureConversionShader
