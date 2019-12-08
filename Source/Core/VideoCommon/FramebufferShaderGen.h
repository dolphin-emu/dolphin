// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/CommonTypes.h"

enum class EFBReinterpretType;
enum class TextureFormat;

namespace FramebufferShaderGen
{
std::string GenerateScreenQuadVertexShader();
std::string GeneratePassthroughGeometryShader(u32 num_tex, u32 num_colors);
std::string GenerateTextureCopyVertexShader();
std::string GenerateTextureCopyPixelShader();
std::string GenerateResolveDepthPixelShader(u32 samples);
std::string GenerateClearVertexShader();
std::string GenerateEFBPokeVertexShader();
std::string GenerateColorPixelShader();
std::string GenerateFormatConversionShader(EFBReinterpretType convtype, u32 samples);
std::string GenerateTextureReinterpretShader(TextureFormat from_format, TextureFormat to_format);
std::string GenerateEFBRestorePixelShader();
std::string GenerateImGuiVertexShader();
std::string GenerateImGuiPixelShader();

}  // namespace FramebufferShaderGen
