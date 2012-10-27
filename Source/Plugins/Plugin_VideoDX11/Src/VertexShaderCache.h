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

#include "VertexShaderGen.h"

#include "D3DBase.h"
#include "D3DBlob.h"

#include <map>

namespace DX11 {

class VertexShaderCache
{
public:
	static void Init();
	static void Clear();
	static void Shutdown();
	static bool SetShader(u32 components); // TODO: Should be renamed to LoadShader

	static ID3D11VertexShader* GetActiveShader() { return last_entry->shader; }
	static D3DBlob* GetActiveShaderBytecode() { return last_entry->bytecode; }
	static ID3D11Buffer* &GetConstantBuffer();

	static ID3D11VertexShader* GetSimpleVertexShader();
	static ID3D11VertexShader* GetClearVertexShader();
	static ID3D11InputLayout* GetSimpleInputLayout();
	static ID3D11InputLayout* GetClearInputLayout();

	static bool VertexShaderCache::InsertByteCode(const VERTEXSHADERUID &uid, D3DBlob* bcodeblob);

private:
	struct VSCacheEntry
	{ 
		ID3D11VertexShader* shader;
		D3DBlob* bytecode; // needed to initialize the input layout

		VERTEXSHADERUIDSAFE safe_uid;
		std::string code;

		VSCacheEntry() : shader(NULL), bytecode(NULL) {}
		void SetByteCode(D3DBlob* blob)
		{
			SAFE_RELEASE(bytecode);
			bytecode = blob;
			blob->AddRef();
		}
		void Destroy()
		{
			SAFE_RELEASE(shader);
			SAFE_RELEASE(bytecode);
		}
	};
	typedef std::map<VERTEXSHADERUID, VSCacheEntry> VSCache;
	
	static VSCache vshaders;
	static const VSCacheEntry* last_entry;
	static VERTEXSHADERUID last_uid;
};

}  // namespace DX11

#endif  // _VERTEXSHADERCACHE_H
