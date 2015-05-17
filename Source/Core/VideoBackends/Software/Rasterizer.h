// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/ChunkFile.h"

struct OutputVertexData;

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

		float GetValue(float dx, float dy) { return f0 + (dfdx * dx) + (dfdy * dy); }
		void DoState(PointerWrap &p)
		{
			p.Do(dfdx);
			p.Do(dfdy);
			p.Do(f0);
		}
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

	void DoState(PointerWrap &p);
}
