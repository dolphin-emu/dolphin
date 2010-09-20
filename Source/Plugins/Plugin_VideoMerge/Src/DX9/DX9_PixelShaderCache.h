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

#ifndef _PIXELSHADERCACHE_H
#define _PIXELSHADERCACHE_H

#include "Common.h"
#include "LinearDiskCache.h"
#include "DX9_D3DBase.h"

#include <map>

#include "PixelShaderGen.h"
#include "VertexShaderGen.h"

#include "../PixelShaderCache.h"

namespace DX9
{

typedef u32 tevhash;

tevhash GetCurrentTEV();

class PixelShaderCache : public ::PixelShaderCacheBase
{
private:
	struct PSCacheEntry
	{
		LPDIRECT3DPIXELSHADER9 shader;
		bool owns_shader;
		int frameCount;
#if defined(_DEBUG) || defined(DEBUGFAST)
		std::string code;
#endif
		PSCacheEntry() : shader(NULL), owns_shader(true), frameCount(0) {}
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
	static void Clear();

public:
	PixelShaderCache();
	~PixelShaderCache();

	void SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
	void SetPSConstant4fv(unsigned int const_number, const float* f);
	void SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float* f);

	bool SetShader(bool dstAlpha);

	static bool InsertByteCode(const PIXELSHADERUID &uid, const u8 *bytecode, int bytecodelen, bool activate);
	static LPDIRECT3DPIXELSHADER9 GetColorMatrixProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetColorCopyProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetDepthMatrixProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetClearProgram();	
	
#if defined(_DEBUG) || defined(DEBUGFAST)
	static std::string GetCurrentShaderCode();
#endif
	static LPDIRECT3DPIXELSHADER9 CompileCgShader(const char *pstrprogram);
};

}

#endif  // _PIXELSHADERCACHE_H
