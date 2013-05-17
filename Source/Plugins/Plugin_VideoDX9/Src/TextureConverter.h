// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _TEXTURECONVERTER_H_
#define _TEXTURECONVERTER_H_

#include "VideoCommon.h"
#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "D3DShader.h"

namespace DX9
{

// Converts textures between formats using shaders
// TODO: support multiple texture formats
namespace TextureConverter
{

void Init();
void Shutdown();

void EncodeToRamYUYV(LPDIRECT3DTEXTURE9 srcTexture, const TargetRectangle& sourceRc,
					 u8* destAddr, int dstWidth, int dstHeight,float Gamma);

void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, LPDIRECT3DTEXTURE9 destTexture);

// returns size of the encoded data (in bytes)
int EncodeToRamFromTexture(u32 address,LPDIRECT3DTEXTURE9 source_texture, u32 SourceW, u32 SourceH, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf, const EFBRectangle& source);


}

}  // namespace DX9

#endif // _TEXTURECONVERTER_H_
