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

// Common
#include "Common.h"
#include "FileUtil.h"
#include "LinearDiskCache.h"

// VideoCommon
#include "Statistics.h"
#include "VideoConfig.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"
#include "ImageWrite.h"
#include "PixelShaderGen.h"
#include "PixelShaderManager.h"

// DX11
#include "DX11_D3DBase.h"
#include "DX11_D3DShader.h"
#include "DX11_PixelShaderCache.h"

#include <D3Dcompiler.h>

#include "../Main.h"

namespace DX11
{

PixelShaderCache::PSCache PixelShaderCache::PixelShaders;
const PixelShaderCache::PSCacheEntry* PixelShaderCache::last_entry;

LinearDiskCache g_ps_disk_cache;

ID3D11PixelShader* s_ColorMatrixProgram = NULL;
ID3D11PixelShader* s_ColorCopyProgram = NULL;
ID3D11PixelShader* s_DepthMatrixProgram = NULL;
ID3D11PixelShader* s_ClearProgram = NULL;

const char clear_program_code[] = {
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float4 incol0 : COLOR0){\n"
	"ocol0 = incol0;\n"
	"}\n"
};

const char color_copy_program_code[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2D Tex0 : register(t0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float2 uv0 : TEXCOORD0){\n"
	"ocol0 = Tex0.Sample(samp0,uv0);\n"
	"}\n"
};

const char color_matrix_program_code[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2D Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[5] : register(c0);\n"
	"void main(\n" 
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	" in float2 uv0 : TEXCOORD0){\n"
	"float4 texcol = Tex0.Sample(samp0,uv0);\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

const char depth_matrix_program[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2D Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[5] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	" in float4 pos : SV_Position,\n"
	" in float2 uv0 : TEXCOORD0){\n"
	"float4 texcol = Tex0.Sample(samp0,uv0);\n"
	"float4 EncodedDepth = frac((texcol.r * (16777215.0f/16777216.0f)) * float4(1.0f,255.0f,255.0f*255.0f,255.0f*255.0f*255.0f));\n"
	"texcol = float4((EncodedDepth.rgb * (16777216.0f/16777215.0f)),1.0f);\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

ID3D11PixelShader* PixelShaderCache::GetColorMatrixProgram()
{
	return s_ColorMatrixProgram;
}

ID3D11PixelShader* PixelShaderCache::GetDepthMatrixProgram()
{
	return s_DepthMatrixProgram;
}

ID3D11PixelShader* PixelShaderCache::GetColorCopyProgram()
{
	return s_ColorCopyProgram;
}

ID3D11PixelShader* PixelShaderCache::GetClearProgram()
{
	return s_ClearProgram;
}

// HACK to avoid some invasive VideoCommon changes
// these values are hardcoded, they depend on internal D3DCompile behavior; TODO: Solve this with D3DReflect or something
// offset given in floats, table index is float4
unsigned int ps_constant_offset_table[] = {
	0, 4, 8, 12,					// C_COLORS, 16
	16, 20, 24, 28,					// C_KCOLORS, 16
	32,								// C_ALPHA, 4
	36, 40, 44, 48, 52, 56, 60, 64,	// C_TEXDIMS, 32
	68, 72,							// C_ZBIAS, 8
	76, 80,							// C_INDTEXSCALE, 8
	84, 88, 92, 96, 100, 104,		// C_INDTEXMTX, 24
	108, 112,						// C_FOG, 8
	116, 120, 124 ,128,				// C_COLORMATRIX, 16	
	132, 136, 140, 144, 148,        // C_PLIGHTS0, 20
	152, 156, 160, 164, 168,		// C_PLIGHTS1, 20
	172, 176, 180, 184, 188,		// C_PLIGHTS2, 20		
	192, 196, 200, 204, 208,		// C_PLIGHTS3, 20
	212, 216, 220, 224, 228,		// C_PLIGHTS4, 20
	232, 236, 240, 244, 248,		// C_PLIGHTS5, 20
	252, 256, 260, 264, 268,		// C_PLIGHTS6, 20
	272, 276, 280, 284, 288,		// C_PLIGHTS7, 20	
	292, 296, 300, 304,				// C_PMATERIALS, 16
};
void PixelShaderCache::SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	D3D::gfxstate->psconstants[ps_constant_offset_table[const_number]  ] = f1;
	D3D::gfxstate->psconstants[ps_constant_offset_table[const_number]+1] = f2;
	D3D::gfxstate->psconstants[ps_constant_offset_table[const_number]+2] = f3;
	D3D::gfxstate->psconstants[ps_constant_offset_table[const_number]+3] = f4;
	D3D::gfxstate->pscbufchanged = true;
}

void PixelShaderCache::SetPSConstant4fv(unsigned int const_number, const float* f)
{
	memcpy(&D3D::gfxstate->psconstants[ps_constant_offset_table[const_number]], f, sizeof(float)*4);
	D3D::gfxstate->pscbufchanged = true;
}

void PixelShaderCache::SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float* f)
{
	memcpy(&D3D::gfxstate->psconstants[ps_constant_offset_table[const_number]], f, sizeof(float)*4*count);
	D3D::gfxstate->pscbufchanged = true;
}

// this class will load the precompiled shaders into our cache
class PixelShaderCacheInserter : public LinearDiskCacheReader
{
public:
	void Read(const u8* key, int key_size, const u8* value, int value_size)
	{
		PIXELSHADERUID uid;
		if (key_size != sizeof(uid)) {
			ERROR_LOG(VIDEO, "Wrong key size in pixel shader cache");
			return;
		}
		memcpy(&uid, key, key_size);
		PixelShaderCache::InsertByteCode(uid, (void*)value, value_size);
	}
};

PixelShaderCache::PixelShaderCache()
{
	// used when drawing clear quads
	s_ClearProgram = D3D::CompileAndCreatePixelShader(clear_program_code, sizeof(clear_program_code));	
	CHECK(s_ClearProgram!=NULL, "Create clear pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ClearProgram, "clear pixel shader");

	// used when copying/resolving the color buffer
	s_ColorCopyProgram = D3D::CompileAndCreatePixelShader(color_copy_program_code, sizeof(color_copy_program_code));
	CHECK(s_ColorCopyProgram!=NULL, "Create color copy pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ClearProgram, "color copy pixel shader");

	// used for color conversion
	s_ColorMatrixProgram = D3D::CompileAndCreatePixelShader(color_matrix_program_code, sizeof(color_matrix_program_code));
	CHECK(s_ColorMatrixProgram!=NULL, "Create color matrix pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ClearProgram, "color matrix pixel shader");

	// used for depth copy
	s_DepthMatrixProgram = D3D::CompileAndCreatePixelShader(depth_matrix_program, sizeof(depth_matrix_program));
	CHECK(s_DepthMatrixProgram!=NULL, "Create depth matrix pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ClearProgram, "depth matrix pixel shader");

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx11-%s-ps.cache", File::GetUserPath(D_SHADERCACHE_IDX), g_globals->unique_id);
	PixelShaderCacheInserter inserter;
	g_ps_disk_cache.OpenAndRead(cache_filename, &inserter);
}

// ONLY to be used during shutdown.
void PixelShaderCache::Clear()
{
	for (PSCache::iterator iter = PixelShaders.begin(); iter != PixelShaders.end(); iter++)
		iter->second.Destroy();
	PixelShaders.clear(); 
}

PixelShaderCache::~PixelShaderCache()
{
	SAFE_RELEASE(s_ColorMatrixProgram);
	SAFE_RELEASE(s_ColorCopyProgram);
	SAFE_RELEASE(s_DepthMatrixProgram);
	SAFE_RELEASE(s_ClearProgram);
	
	Clear();
	g_ps_disk_cache.Sync();
	g_ps_disk_cache.Close();
}

bool PixelShaderCache::SetShader(bool dstAlpha)
{
	PIXELSHADERUID uid;
	GetPixelShaderId(&uid, dstAlpha);

	// check if the shader is already set
	if (uid == last_pixel_shader_uid && PixelShaders[uid].frameCount == frameCount)
	{
		PSCache::const_iterator iter = PixelShaders.find(uid);
		return (iter != PixelShaders.end() && iter->second.shader);
	}

	memcpy(&last_pixel_shader_uid, &uid, sizeof(PIXELSHADERUID));

	// check if the shader is already in the cache
	PSCache::iterator iter = PixelShaders.find(uid);
	if (iter != PixelShaders.end())
	{
		iter->second.frameCount = frameCount;
		const PSCacheEntry &entry = iter->second;
		last_entry = &entry;
		
		D3D::gfxstate->SetPShader(entry.shader);
		return (entry.shader != NULL);
	}

	// need to compile a new shader
	const char* code = GeneratePixelShaderCode(dstAlpha, API_D3D11);

	D3DBlob* pbytecode;
	if (!D3D::CompilePixelShader(code, (unsigned int)strlen(code), &pbytecode))
	{
		PanicAlert("Failed to compile Pixel Shader:\n\n%s", code);
		return false;
	}

	// insert the bytecode into the caches
	g_ps_disk_cache.Append((u8*)&uid, sizeof(uid), (const u8*)pbytecode->Data(), pbytecode->Size());
	g_ps_disk_cache.Sync();

	bool result = InsertByteCode(uid, pbytecode->Data(), pbytecode->Size());
	D3D::gfxstate->SetPShader(last_entry->shader);
	pbytecode->Release();
	return result;
}

bool PixelShaderCache::InsertByteCode(const PIXELSHADERUID &uid, void* bytecode, unsigned int bytecodelen)
{
	ID3D11PixelShader* shader = D3D::CreatePixelShaderFromByteCode(bytecode, bytecodelen);
	if (shader == NULL)
	{
		PanicAlert("Failed to create pixel shader at %s %d\n", __FILE__, __LINE__);
		return false;
	}
	// TODO: Somehow make the debug name a bit more specific
	D3D::SetDebugObjectName((ID3D11DeviceChild*)shader, "a pixel shader of PixelShaderCache");

	// make an entry in the table
	PSCacheEntry newentry;
	newentry.shader = shader;
	newentry.frameCount = frameCount;
	PixelShaders[uid] = newentry;
	last_entry = &PixelShaders[uid];

	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, PixelShaders.size());

	return true;
}

}
