// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

		std::string code;

		PSCacheEntry() : shader(NULL), owns_shader(true) {}
		void Destroy()
		{
			if (shader && owns_shader)
				shader->Release();
			shader = NULL;
		}
	};

	typedef std::map<PixelShaderUid, PSCacheEntry> PSCache;

	static PSCache PixelShaders;
	static const PSCacheEntry *last_entry;
	static PixelShaderUid last_uid;
	static UidChecker<PixelShaderUid,PixelShaderCode> pixel_uid_checker;

	static void Clear();

public:
	static void Init();
	static void Shutdown();
	static bool SetShader(DSTALPHA_MODE dstAlphaMode, u32 componets);
	static bool InsertByteCode(const PixelShaderUid &uid, const u8 *bytecode, int bytecodelen, bool activate);
	static LPDIRECT3DPIXELSHADER9 GetColorMatrixProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetColorCopyProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetDepthMatrixProgram(int SSAAMode, bool depthConversion);
	static LPDIRECT3DPIXELSHADER9 GetClearProgram();	
	static LPDIRECT3DPIXELSHADER9 ReinterpRGBA6ToRGB8();
	static LPDIRECT3DPIXELSHADER9 ReinterpRGB8ToRGBA6();
};

}  // namespace DX9