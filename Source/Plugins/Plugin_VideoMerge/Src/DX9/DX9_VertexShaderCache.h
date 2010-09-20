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

#ifndef _VERTEXSHADERCACHE_H
#define _VERTEXSHADERCACHE_H

#include "DX9_D3DBase.h"

#include <map>
#include <string>

#include "DX9_D3DBase.h"
#include "VertexShaderGen.h"

#include "../VertexShaderCache.h"

namespace DX9
{

class VertexShaderCache : public ::VertexShaderCacheBase
{
private:
	struct VSCacheEntry
	{ 
		LPDIRECT3DVERTEXSHADER9 shader;
		int frameCount;
#if defined(_DEBUG) || defined(DEBUGFAST)
		std::string code;
#endif
		VSCacheEntry() : shader(NULL), frameCount(0) {}
		void Destroy()
		{
			if (shader)
				shader->Release();
			shader = NULL;
		}
	};

	typedef std::map<VERTEXSHADERUID, VSCacheEntry> VSCache;

	static VSCache vshaders;
	static const VSCacheEntry *last_entry;
	static void Clear();

public:
	VertexShaderCache();
	~VertexShaderCache();

	void SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
	void SetVSConstant4fv(unsigned int const_number, const float *f);
	void SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f);
	void SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f);
	
	bool SetShader(u32 components);
	
	static LPDIRECT3DVERTEXSHADER9 GetSimpleVertexShader(int level);
	static LPDIRECT3DVERTEXSHADER9 GetClearVertexShader();	
	static bool InsertByteCode(const VERTEXSHADERUID &uid, const u8 *bytecode, int bytecodelen, bool activate);
#if defined(_DEBUG) || defined(DEBUGFAST)
	static std::string GetCurrentShaderCode();
#endif
	static LPDIRECT3DVERTEXSHADER9 CompileCgShader(const char *pstrprogram);
};

}

#endif  // _VERTEXSHADERCACHE_H
