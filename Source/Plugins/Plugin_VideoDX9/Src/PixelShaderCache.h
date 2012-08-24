// Copyright (C) 2003 Dolphin Project.

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

#pragma once

#include "Common.h"
#include "LinearDiskCache.h"
#include "D3DBase.h"

#include <map>

#include "PixelShaderGen.h"
#include "VertexShaderGen.h"

namespace DX9
{

typedef u32 tevhash;

tevhash GetCurrentTEV();

class PixelShaderCache
{
private:
	struct PSCacheEntry
	{
		LPDIRECT3DPIXELSHADER9 shader;
		bool owns_shader;

		PIXELSHADERUIDSAFE safe_uid;
		std::string code;

		PSCacheEntry() : shader(NULL), owns_shader(true) {}
		void Destroy()
		{
			if (shader && owns_shader)
				shader->Release();
			shader = NULL;
		}
	};

	typedef std::map<PIXELSHADERUID, PSCacheEntry> PSCache;

	static PSCache PixelShaders;
	static const PSCacheEntry *last_entry;
	static PIXELSHADERUID last_uid;
	static void Clear();

public:
	static void Init();
	static void Shutdown();
	static bool SetShader(DSTALPHA_MODE dstAlphaMode, u32 componets);
	static bool InsertByteCode(const PIXELSHADERUID &uid, const u8 *bytecode, int bytecodelen, bool activate);
	static LPDIRECT3DPIXELSHADER9 GetColorMatrixProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetColorCopyProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetDepthMatrixProgram(int SSAAMode, bool depthConversion);
	static LPDIRECT3DPIXELSHADER9 GetClearProgram();	
	static LPDIRECT3DPIXELSHADER9 ReinterpRGBA6ToRGB8();
	static LPDIRECT3DPIXELSHADER9 ReinterpRGB8ToRGBA6();
};

}  // namespace DX9