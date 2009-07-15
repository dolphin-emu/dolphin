// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _TEXTURECONVERTER_H_
#define _TEXTURECONVERTER_H_

#include "VideoCommon.h"
#include "GLUtil.h"

// Converts textures between formats
// TODO: support multiple texture formats
namespace TextureConverter
{

void Init();
void Shutdown();

void EncodeToRam(u32 address, bool bFromZBuffer, bool bIsIntensityFmt,
				 u32 copyfmt, bool bScaleByHalf, const EFBRectangle& source);

void EncodeToRamYUYV(GLuint srcTexture, const TargetRectangle& sourceRc,
					 u8* destAddr, int dstWidth, int dstHeight);

void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, GLuint destTexture);

}

#endif // _TEXTURECONVERTER_H_
