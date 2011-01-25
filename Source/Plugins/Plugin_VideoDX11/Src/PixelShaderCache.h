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

class PixelShaderCache
{
public:
	static void Init();
	static void Clear();
	static void Shutdown();
	static bool SetShader(DSTALPHA_MODE dstAlphaMode, u32 components); // TODO: Should be renamed to LoadShader
	static bool InsertByteCode(const PIXELSHADERUID &uid, const void* bytecode, unsigned int bytecodelen);

	static ID3D11PixelShader* GetActiveShader() { return last_entry->shader; }
	static ID3D11Buffer* &GetConstantBuffer();

	static ID3D11PixelShader* GetColorMatrixProgram(bool multisampled);
	static ID3D11PixelShader* GetColorCopyProgram(bool multisampled);
	static ID3D11PixelShader* GetDepthMatrixProgram(bool multisampled);
	static ID3D11PixelShader* GetClearProgram();

	static void InvalidateMSAAShaders();

private:
	struct PSCacheEntry
	{
		ID3D11PixelShader* shader;
		int frameCount;

		PSCacheEntry() : shader(NULL), frameCount(0) {}
		void Destroy() { SAFE_RELEASE(shader); }
	};

	typedef std::map<PIXELSHADERUID, PSCacheEntry> PSCache;

	static PSCache PixelShaders;
	static const PSCacheEntry* last_entry;
};
