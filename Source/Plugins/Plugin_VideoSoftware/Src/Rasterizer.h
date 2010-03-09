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

#ifndef _RASTERIZER_H_
#define _RASTERIZER_H_

#include "NativeVertexFormat.h"

namespace Rasterizer
{
    void Init();

    void DrawTriangleFrontFace(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2);

    void SetScissor();

    void SetTevReg(int reg, int comp, bool konst, s16 color);

    struct Slope
    {
        float dfdx;
        float dfdy;
        float f0;
        float x0;
        float y0;
        float GetValue(s32 x, s32 y) { return f0 + (dfdx * (x - x0)) + (dfdy * (y - y0)); }
    };

	struct RasterBlockPixel
	{
		float InvW;
		float Uv[8][2];
	};

	struct RasterBlock
	{
		RasterBlockPixel Pixel[2][2];
		s32 IndirectLod[4];
		bool IndirectLinear[4];
		s32 TextureLod[16];
		bool TextureLinear[16];
	};
    
}

#endif
