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

#include "Common.h"
#include "FileUtil.h"
#include "LinearDiskCache.h"

#include "Globals.h"
#include "D3DBase.h"
#include "D3Dcompiler.h"
#include "D3DShader.h"
#include "Statistics.h"
#include "VideoConfig.h"
#include "PixelShaderGen.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"
#include "ImageWrite.h"
#include "Debugger.h"

extern int frameCount;

PixelShaderCache::PSCache PixelShaderCache::PixelShaders;
const PixelShaderCache::PSCacheEntry* PixelShaderCache::last_entry;

LinearDiskCache<PIXELSHADERUID, u8> g_ps_disk_cache;

ID3D11PixelShader* s_ColorMatrixProgram[2] = {NULL};
ID3D11PixelShader* s_ColorCopyProgram[2] = {NULL};
ID3D11PixelShader* s_DepthMatrixProgram[2] = {NULL};
ID3D11PixelShader* s_ClearProgram = NULL;

float psconstants[C_PENVCONST_END*4];
bool pscbufchanged = true;
ID3D11Buffer* pscbuf = NULL;

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

// TODO: Improve sampling algorithm!
const char color_copy_program_code_msaa[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DMS<float4, %d> Tex0 : register(t0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	"in float2 uv0 : TEXCOORD0){\n"
	"int width, height, samples;\n"
	"Tex0.GetDimensions(width, height, samples);\n"
	"ocol0 = 0;\n"
	"for(int i = 0; i < samples; ++i)\n"
	"	ocol0 += Tex0.Load(int2(uv0.x*(width), uv0.y*(height)), i);\n"
	"ocol0 /= samples;\n"
	"}\n"
};

const char color_matrix_program_code[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2D Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n" 
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	" in float2 uv0 : TEXCOORD0){\n"
	"float4 texcol = Tex0.Sample(samp0,uv0);\n"
	"texcol = round(texcol * cColMatrix[5])*cColMatrix[6];\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

const char color_matrix_program_code_msaa[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DMS<float4, %d> Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n" 
	"out float4 ocol0 : SV_Target,\n"
	"in float4 pos : SV_Position,\n"
	" in float2 uv0 : TEXCOORD0){\n"
	"int width, height, samples;\n"
	"Tex0.GetDimensions(width, height, samples);\n"
	"float4 texcol = 0;\n"
	"for(int i = 0; i < samples; ++i)\n"
	"	texcol += Tex0.Load(int2(uv0.x*(width), uv0.y*(height)), i);\n"
	"texcol /= samples;\n"
	"texcol = round(texcol * cColMatrix[5])*cColMatrix[6];\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

const char depth_matrix_program[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2D Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	" in float4 pos : SV_Position,\n"
	" in float2 uv0 : TEXCOORD0){\n"
	"float4 texcol = Tex0.Sample(samp0,uv0);\n"
	"float4 EncodedDepth = frac((texcol.r * (16777215.0f/16777216.0f)) * float4(1.0f,256.0f,256.0f*256.0f,1.0f));\n"
	"texcol = round(EncodedDepth * (16777216.0f/16777215.0f) * float4(255.0f,255.0f,255.0f,15.0f)) / float4(255.0f,255.0f,255.0f,15.0f);\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

const char depth_matrix_program_msaa[] = {
	"sampler samp0 : register(s0);\n"
	"Texture2DMS<float4, %d> Tex0 : register(t0);\n"
	"uniform float4 cColMatrix[7] : register(c0);\n"
	"void main(\n"
	"out float4 ocol0 : SV_Target,\n"
	" in float4 pos : SV_Position,\n"
	" in float2 uv0 : TEXCOORD0){\n"
	"int width, height, samples;\n"
	"Tex0.GetDimensions(width, height, samples);\n"
	"float4 texcol = 0;\n"
	"for(int i = 0; i < samples; ++i)\n"
	"	texcol += Tex0.Load(int2(uv0.x*(width), uv0.y*(height)), i);\n"
	"texcol /= samples;\n"
	"float4 EncodedDepth = frac((texcol.r * (16777215.0f/16777216.0f)) * float4(1.0f,256.0f,256.0f*256.0f,16.0f));\n"
	"texcol = round(EncodedDepth * (16777216.0f/16777215.0f) * float4(255.0f,255.0f,255.0f,15.0f)) / float4(255.0f,255.0f,255.0f,15.0f);\n"
	"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
	"}\n"
};

ID3D11PixelShader* PixelShaderCache::GetColorCopyProgram(bool multisampled)
{
	if (!multisampled || D3D::GetAAMode(g_ActiveConfig.iMultisampleMode).Count == 1) return s_ColorCopyProgram[0];
	else if (s_ColorCopyProgram[1]) return s_ColorCopyProgram[1];
	else
	{
		// TODO: recreate shader on AA mode change!
		// create MSAA shader for current AA mode
		char buf[1024];
		int l = sprintf_s(buf, 1024, color_copy_program_code_msaa, D3D::GetAAMode(g_ActiveConfig.iMultisampleMode));
		s_ColorCopyProgram[1] = D3D::CompileAndCreatePixelShader(buf, l);
		CHECK(s_ColorCopyProgram[1]!=NULL, "Create color copy MSAA pixel shader");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ColorCopyProgram[1], "color copy MSAA pixel shader");
		return s_ColorCopyProgram[1];
	}
}

ID3D11PixelShader* PixelShaderCache::GetColorMatrixProgram(bool multisampled)
{
	if (!multisampled || D3D::GetAAMode(g_ActiveConfig.iMultisampleMode).Count == 1) return s_ColorMatrixProgram[0];
	else if (s_ColorMatrixProgram[1]) return s_ColorMatrixProgram[1];
	else
	{
		// TODO: recreate shader on AA mode change!
		// create MSAA shader for current AA mode
		char buf[1024];
		int l = sprintf_s(buf, 1024, color_matrix_program_code_msaa, D3D::GetAAMode(g_ActiveConfig.iMultisampleMode));
		s_ColorMatrixProgram[1] = D3D::CompileAndCreatePixelShader(buf, l);
		CHECK(s_ColorMatrixProgram[1]!=NULL, "Create color matrix MSAA pixel shader");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ColorMatrixProgram[1], "color matrix MSAA pixel shader");
		return s_ColorMatrixProgram[1];
	}
}

ID3D11PixelShader* PixelShaderCache::GetDepthMatrixProgram(bool multisampled)
{
	if (!multisampled || D3D::GetAAMode(g_ActiveConfig.iMultisampleMode).Count == 1) return s_DepthMatrixProgram[0];
	else if (s_DepthMatrixProgram[1]) return s_DepthMatrixProgram[1];
	else
	{
		// TODO: recreate shader on AA mode change!
		// create MSAA shader for current AA mode
		char buf[1024];
		int l = sprintf_s(buf, 1024, depth_matrix_program_msaa, D3D::GetAAMode(g_ActiveConfig.iMultisampleMode));
		s_DepthMatrixProgram[1] = D3D::CompileAndCreatePixelShader(buf, l);
		CHECK(s_DepthMatrixProgram[1]!=NULL, "Create depth matrix MSAA pixel shader");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)s_DepthMatrixProgram[1], "depth matrix MSAA pixel shader");
		return s_DepthMatrixProgram[1];
	}
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
	116, 120, 124, 128, 132,		// C_PLIGHTS0, 20
	136, 140, 144, 148, 152,		// C_PLIGHTS1, 20
	156, 160, 164, 168, 172,		// C_PLIGHTS2, 20
	176, 180, 184, 188, 192,		// C_PLIGHTS3, 20		
	196, 200, 204, 208, 212,		// C_PLIGHTS4, 20
	216, 220, 224, 228, 232,		// C_PLIGHTS5, 20
	236, 240, 244, 248, 252,		// C_PLIGHTS6, 20
	256, 260, 264, 268, 272,		// C_PLIGHTS7, 20
	276, 280, 284, 288, 			// C_PMATERIALS, 16	
};
void SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	psconstants[ps_constant_offset_table[const_number]  ] = f1;
	psconstants[ps_constant_offset_table[const_number]+1] = f2;
	psconstants[ps_constant_offset_table[const_number]+2] = f3;
	psconstants[ps_constant_offset_table[const_number]+3] = f4;
	pscbufchanged = true;
}

void SetPSConstant4fv(unsigned int const_number, const float* f)
{
	memcpy(&psconstants[ps_constant_offset_table[const_number]], f, sizeof(float)*4);
	pscbufchanged = true;
}

void SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float* f)
{
	memcpy(&psconstants[ps_constant_offset_table[const_number]], f, sizeof(float)*4*count);
	pscbufchanged = true;
}

ID3D11Buffer* &PixelShaderCache::GetConstantBuffer()
{
	// TODO: divide the global variables of the generated shaders into about 5 constant buffers to speed this up
	if (pscbufchanged)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		D3D::context->Map(pscbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, psconstants, sizeof(psconstants));
		D3D::context->Unmap(pscbuf, 0);
		pscbufchanged = false;
	}
	return pscbuf;
}

// this class will load the precompiled shaders into our cache
class PixelShaderCacheInserter : public LinearDiskCacheReader<PIXELSHADERUID, u8>
{
public:
	void Read(const PIXELSHADERUID &key, const u8 *value, u32 value_size)
	{
		PixelShaderCache::InsertByteCode(key, value, value_size);
	}
};

void PixelShaderCache::Init()
{
	unsigned int cbsize = ((sizeof(psconstants))&(~0xf))+0x10; // must be a multiple of 16
	D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(cbsize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	D3D::device->CreateBuffer(&cbdesc, NULL, &pscbuf);
	CHECK(pscbuf!=NULL, "Create pixel shader constant buffer");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)pscbuf, "pixel shader constant buffer used to emulate the GX pipeline");

	// used when drawing clear quads
	s_ClearProgram = D3D::CompileAndCreatePixelShader(clear_program_code, sizeof(clear_program_code));	
	CHECK(s_ClearProgram!=NULL, "Create clear pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ClearProgram, "clear pixel shader");

	// used when copying/resolving the color buffer
	s_ColorCopyProgram[0] = D3D::CompileAndCreatePixelShader(color_copy_program_code, sizeof(color_copy_program_code));
	CHECK(s_ColorCopyProgram[0]!=NULL, "Create color copy pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ColorCopyProgram[0], "color copy pixel shader");

	// used for color conversion
	s_ColorMatrixProgram[0] = D3D::CompileAndCreatePixelShader(color_matrix_program_code, sizeof(color_matrix_program_code));
	CHECK(s_ColorMatrixProgram[0]!=NULL, "Create color matrix pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ColorMatrixProgram[0], "color matrix pixel shader");

	// used for depth copy
	s_DepthMatrixProgram[0] = D3D::CompileAndCreatePixelShader(depth_matrix_program, sizeof(depth_matrix_program));
	CHECK(s_DepthMatrixProgram[0]!=NULL, "Create depth matrix pixel shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)s_DepthMatrixProgram[0], "depth matrix pixel shader");

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	SETSTAT(stats.numPixelShadersCreated, 0);
	SETSTAT(stats.numPixelShadersAlive, 0);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx11-%s-ps.cache", File::GetUserPath(D_SHADERCACHE_IDX), globals->unique_id);
	PixelShaderCacheInserter inserter;
	g_ps_disk_cache.OpenAndRead(cache_filename, inserter);
}

// ONLY to be used during shutdown.
void PixelShaderCache::Clear()
{
	for (PSCache::iterator iter = PixelShaders.begin(); iter != PixelShaders.end(); iter++)
		iter->second.Destroy();
	PixelShaders.clear(); 
}

// Used in Swap() when AA mode has changed
void PixelShaderCache::InvalidateMSAAShaders()
{
	SAFE_RELEASE(s_ColorCopyProgram[1]);
	SAFE_RELEASE(s_ColorMatrixProgram[1]);
	SAFE_RELEASE(s_DepthMatrixProgram[1]);
}

void PixelShaderCache::Shutdown()
{
	SAFE_RELEASE(pscbuf);

	SAFE_RELEASE(s_ClearProgram);
	for (int i = 0; i < 2; ++i)
	{
		SAFE_RELEASE(s_ColorCopyProgram[i]);
		SAFE_RELEASE(s_ColorMatrixProgram[i]);
		SAFE_RELEASE(s_DepthMatrixProgram[i]);
	}
	
	Clear();
	g_ps_disk_cache.Sync();
	g_ps_disk_cache.Close();
}

bool PixelShaderCache::SetShader(DSTALPHA_MODE dstAlphaMode, u32 components)
{
	PIXELSHADERUID uid;
	GetPixelShaderId(&uid, dstAlphaMode);

	// Check if the shader is already set
	if (uid == last_pixel_shader_uid && PixelShaders[uid].frameCount == frameCount)
	{
		PSCache::const_iterator iter = PixelShaders.find(uid);
		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE,true);
		return (iter != PixelShaders.end() && iter->second.shader);
	}

	memcpy(&last_pixel_shader_uid, &uid, sizeof(PIXELSHADERUID));

	// Check if the shader is already in the cache
	PSCache::iterator iter;
	iter = PixelShaders.find(uid);
	if (iter != PixelShaders.end())
	{
		iter->second.frameCount = frameCount;
		const PSCacheEntry &entry = iter->second;
		last_entry = &entry;
		
		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE,true);
		return (entry.shader != NULL);
	}

	// Need to compile a new shader
	const char* code = GeneratePixelShaderCode(dstAlphaMode, API_D3D11, components);

	D3DBlob* pbytecode;
	if (!D3D::CompilePixelShader(code, (unsigned int)strlen(code), &pbytecode))
	{
		PanicAlert("Failed to compile Pixel Shader:\n\n%s", code);
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return false;
	}

	// Insert the bytecode into the caches
	g_ps_disk_cache.Append(uid, pbytecode->Data(), pbytecode->Size());
	g_ps_disk_cache.Sync();

	bool result = InsertByteCode(uid, pbytecode->Data(), pbytecode->Size());
	pbytecode->Release();
	GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	return result;
}

bool PixelShaderCache::InsertByteCode(const PIXELSHADERUID &uid, const void* bytecode, unsigned int bytecodelen)
{
	ID3D11PixelShader* shader = D3D::CreatePixelShaderFromByteCode(bytecode, bytecodelen);
	if (shader == NULL)
	{
		PanicAlert("Failed to create pixel shader at %s %d\n", __FILE__, __LINE__);
		return false;
	}
	// TODO: Somehow make the debug name a bit more specific
	D3D::SetDebugObjectName((ID3D11DeviceChild*)shader, "a pixel shader of PixelShaderCache");

	// Make an entry in the table
	PSCacheEntry newentry;
	newentry.shader = shader;
	newentry.frameCount = frameCount;
	PixelShaders[uid] = newentry;
	last_entry = &PixelShaders[uid];

	if (!shader) {
		// INCSTAT(stats.numPixelShadersFailed);
		return false;
	}

	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, PixelShaders.size());
	return true;
}