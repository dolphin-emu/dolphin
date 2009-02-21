// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _TEXTURECONVERSIONSHADER_H
#define _TEXTURECONVERSIONSHADER_H

#include "Common.h"
#include "TextureDecoder.h"
#include <Cg/cg.h> 

namespace TextureConversionShader
{

u16 GetBlockWidthInTexels(u32 format);
u16 GetBlockHeightInTexels(u32 format);
u16 GetEncodedSampleCount(u32 format);

const char *GenerateEncodingShader(u32 format);

void SetShaderParameters(u32 width, u32 height, u32 offsetX, u32 offsetY, float pixelStride, u32 format);

}

#endif