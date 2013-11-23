// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _TEXTURECONVERTER_H_
#define _TEXTURECONVERTER_H_

#include "VideoCommon.h"
#include "GLUtil.h"

namespace OGL
{

// Converts textures between formats using shaders
// TODO: support multiple texture formats
namespace TextureConverter
{

void Init();
void Shutdown();

void EncodeToRamYUYV(GLuint srcTexture, const TargetRectangle& sourceRc,
					 u8* destAddr, int dstWidth, int dstHeight);

void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, GLuint destTexture);

// returns size of the encoded data (in bytes)
int EncodeToRamFromTexture(u32 address, GLuint source_texture, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf, const EFBRectangle& source);

}

}  // namespace OGL

#endif // _TEXTURECONVERTER_H_
