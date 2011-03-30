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

#include <map>

#include "D3DUtil.h"

class VERTEXSHADERUID;

namespace DX11
{

class VertexShaderCache
{
public:
	static void Init();
	static void Clear();
	static void Shutdown();
	static bool LoadShader(u32 components);

	static SharedPtr<ID3D11VertexShader> GetActiveShader() { return last_entry->shader; }
	static SharedPtr<ID3D10Blob> GetActiveShaderBytecode() { return last_entry->bytecode; }
	static ID3D11Buffer*const& GetConstantBuffer();

	static ID3D11VertexShader* GetSimpleVertexShader();
	static ID3D11VertexShader* GetClearVertexShader();
	static ID3D11InputLayout* GetSimpleInputLayout();
	static ID3D11InputLayout* GetClearInputLayout();

	static bool VertexShaderCache::InsertByteCode(const VERTEXSHADERUID &uid, SharedPtr<ID3D10Blob> bcodeblob);

private:
	struct VSCacheEntry
	{ 
		SharedPtr<ID3D11VertexShader> shader;
		SharedPtr<ID3D10Blob> bytecode; // needed to initialize the input layout
		int frameCount;

		VSCacheEntry()
			: frameCount(0)
		{}

		void SetByteCode(SharedPtr<ID3D10Blob> blob)
		{
			bytecode = blob;
		}
	};
	typedef std::map<VERTEXSHADERUID, VSCacheEntry> VSCache;
	
	static VSCache vshaders;
	static const VSCacheEntry* last_entry;
};

}  // namespace DX11

#endif  // _VERTEXSHADERCACHE_H
