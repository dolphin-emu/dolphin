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

#include "D3DBase.h"

#include <map>

#include "PixelShaderGen.h"
#include "VertexShaderGen.h"

typedef u32 tevhash;

tevhash GetCurrentTEV();

class PixelShaderCache
{
private:
	struct PSCacheEntry
	{
		LPDIRECT3DPIXELSHADER9 shader;
		int frameCount;
#if defined(_DEBUG) || defined(DEBUGFAST)
		std::string code;
#endif
		PSCacheEntry() : shader(NULL), frameCount(0) {}
		void Destroy()
		{
			if (shader)
				shader->Release();
		}
	};

	typedef std::map<PIXELSHADERUID, PSCacheEntry> PSCache;

	static PSCache PixelShaders;
	static const PSCacheEntry *last_entry;

public:
	static void Init();
	static void Cleanup();
	static void Shutdown();
	static bool SetShader(bool dstAlpha);
#if defined(_DEBUG) || defined(DEBUGFAST)
	static std::string GetCurrentShaderCode();
#endif
	static LPDIRECT3DPIXELSHADER9 CompileCgShader(const char *pstrprogram);
};


#endif  // _PIXELSHADERCACHE_H
