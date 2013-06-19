// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "D3DBase.h"

#include <map>
#include <string>

#include "D3DBase.h"
#include "VertexShaderGen.h"

namespace DX9
{

class VertexShaderCache
{
private:
	struct VSCacheEntry
	{ 
		LPDIRECT3DVERTEXSHADER9 shader;

		std::string code;

		VSCacheEntry() : shader(NULL) {}
		void Destroy()
		{
			if (shader)
				shader->Release();
			shader = NULL;
		}
	};

	typedef std::map<VertexShaderUid, VSCacheEntry> VSCache;

	static VSCache vshaders;
	static const VSCacheEntry *last_entry;
	static VertexShaderUid last_uid;

	static UidChecker<VertexShaderUid,VertexShaderCode> vertex_uid_checker;

	static void Clear();

public:
	static void Init();
	static void Shutdown();
	static bool SetShader(u32 components);
	static LPDIRECT3DVERTEXSHADER9 GetSimpleVertexShader(int level);
	static LPDIRECT3DVERTEXSHADER9 GetClearVertexShader();	
	static bool InsertByteCode(const VertexShaderUid &uid, const u8 *bytecode, int bytecodelen, bool activate);

	static std::string GetCurrentShaderCode();
};

}  // namespace DX9