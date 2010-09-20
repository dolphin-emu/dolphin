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

#include <map>
#include <set>

#include "Common.h"
#include "Hash.h"
#include "FileUtil.h"
#include "LinearDiskCache.h"

#include "DX9_D3DBase.h"
#include "DX9_D3DShader.h"
#include "Statistics.h"
#include "VideoConfig.h"
#include "PixelShaderGen.h"
#include "PixelShaderManager.h"
#include "DX9_PixelShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"
#include "ImageWrite.h"

#include "../Main.h"

//#include "Debugger/Debugger.h"

namespace DX9
{

PixelShaderCache::PSCache PixelShaderCache::PixelShaders;
const PixelShaderCache::PSCacheEntry *PixelShaderCache::last_entry;

static LinearDiskCache g_ps_disk_cache;
static std::set<u32> unique_shaders;

#define MAX_SSAA_SHADERS 3

static LPDIRECT3DPIXELSHADER9 s_ColorMatrixProgram[MAX_SSAA_SHADERS];
static LPDIRECT3DPIXELSHADER9 s_ColorCopyProgram[MAX_SSAA_SHADERS];
static LPDIRECT3DPIXELSHADER9 s_DepthMatrixProgram[MAX_SSAA_SHADERS];
static LPDIRECT3DPIXELSHADER9 s_ClearProgram = 0;

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetColorMatrixProgram(int SSAAMode)
{
	return s_ColorMatrixProgram[SSAAMode % MAX_SSAA_SHADERS];
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetDepthMatrixProgram(int SSAAMode)
{
	return s_DepthMatrixProgram[SSAAMode % MAX_SSAA_SHADERS];
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetColorCopyProgram(int SSAAMode)
{
	return s_ColorCopyProgram[SSAAMode % MAX_SSAA_SHADERS];
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetClearProgram()
{
	return s_ClearProgram;
}

void PixelShaderCache::SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	float f[4] = { f1, f2, f3, f4 };
	D3D::dev->SetPixelShaderConstantF(const_number, f, 1);
}

void PixelShaderCache::SetPSConstant4fv(unsigned int const_number, const float *f)
{
	D3D::dev->SetPixelShaderConstantF(const_number, f, 1);
}

void PixelShaderCache::SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	D3D::dev->SetPixelShaderConstantF(const_number, f, count);
}

class PixelShaderCacheInserter : public LinearDiskCacheReader {
public:
	void Read(const u8 *key, int key_size, const u8 *value, int value_size)
	{
		PIXELSHADERUID uid;
		if (key_size != sizeof(uid)) {
			ERROR_LOG(VIDEO, "Wrong key size in pixel shader cache");
			return;
		}
		memcpy(&uid, key, key_size);
		PixelShaderCache::InsertByteCode(uid, value, value_size, false);
	}
};

PixelShaderCache::PixelShaderCache()
{
	//program used for clear screen
	char pprog[3072];
	sprintf(pprog, "void main(\n"
						"out float4 ocol0 : COLOR0,\n"
						" in float4 incol0 : COLOR0){\n"
						"ocol0 = incol0;\n"
						"}\n");
	s_ClearProgram = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));	

	//Used for Copy/resolve the color buffer
	//1 Sample
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
						"void main(\n"
						"out float4 ocol0 : COLOR0,\n"
						"in float2 uv0 : TEXCOORD0){\n"
						"ocol0 = tex2D(samp0,uv0);\n"						
						"}\n");
	s_ColorCopyProgram[0] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));		

	//1 Samples SSAA
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
					"void main(\n"
					"out float4 ocol0 : COLOR0,\n"
					"in float4 uv0 : TEXCOORD0,\n"
					"in float4 uv1 : TEXCOORD1){\n"
					"ocol0 = tex2D(samp0,uv0.xy);\n"
					"}\n");
	s_ColorCopyProgram[1] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));

	//4 Samples SSAA
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"					
					"void main(\n"
					"out float4 ocol0 : COLOR0,\n"
					"in float4 uv0 : TEXCOORD0,\n"
					"in float4 uv1 : TEXCOORD1,\n"
					"in float4 uv2 : TEXCOORD2,\n"
					"in float4 uv3 : TEXCOORD3){\n"
					"ocol0 = (tex2D(samp0,uv1.xy) + tex2D(samp0,uv1.wz) + tex2D(samp0,uv2.xy) + tex2D(samp0,uv2.wz))*0.25;\n"
					"}\n");
	s_ColorCopyProgram[2] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));



	//Color conversion Programs
	//1 sample
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
						"uniform float4 cColMatrix[5] : register(c%d);\n"
						"void main(\n"
						"out float4 ocol0 : COLOR0,\n"
						" in float2 uv0 : TEXCOORD0){\n"
						"float4 texcol = tex2D(samp0,uv0);\n"
						"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
						"}\n",C_COLORMATRIX);
	s_ColorMatrixProgram[0] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));		
	
	//1 samples SSAA
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
					"uniform float4 cColMatrix[5] : register(c%d);\n"
					"void main(\n"
					"out float4 ocol0 : COLOR0,\n"
					"in float4 uv0 : TEXCOORD0,\n"
					"in float4 uv1 : TEXCOORD1){\n"
					"float4 texcol = tex2D(samp0,uv0.xy);\n"
					"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
					"}\n",C_COLORMATRIX);
	s_ColorMatrixProgram[1] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));

	//4 samples SSAA
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
					"uniform float4 cColMatrix[5] : register(c%d);\n"
					"void main(\n"
					"out float4 ocol0 : COLOR0,\n"
					"in float4 uv0 : TEXCOORD0,\n"
					"in float4 uv1 : TEXCOORD1,\n"
					"in float4 uv2 : TEXCOORD2,\n"
					"in float4 uv3 : TEXCOORD3){\n"
					"float4 texcol = (tex2D(samp0,uv1.xy) + tex2D(samp0,uv1.wz) + tex2D(samp0,uv2.xy) + tex2D(samp0,uv2.wz))*0.25f;\n"
					"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
					"}\n",C_COLORMATRIX);
	s_ColorMatrixProgram[2] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));

	//Depth copy programs
	//1 sample
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
						"uniform float4 cColMatrix[5] : register(c%d);\n"
						"void main(\n"
						"out float4 ocol0 : COLOR0,\n"
						" in float2 uv0 : TEXCOORD0){\n"
						"float4 texcol = tex2D(samp0,uv0);\n"
						"float4 EncodedDepth = frac((texcol.r * (16777215.0f/16777216.0f)) * float4(1.0f,255.0f,255.0f*255.0f,255.0f*255.0f*255.0f));\n"
						"texcol = float4((EncodedDepth.rgb * (16777216.0f/16777215.0f)),1.0f);\n"
						"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"
						"}\n",C_COLORMATRIX);
	s_DepthMatrixProgram[0] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));
	
	//1 sample SSAA
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"					
					"uniform float4 cColMatrix[5] : register(c%d);\n"
					"void main(\n"
					"out float4 ocol0 : COLOR0,\n"
					"in float4 uv0 : TEXCOORD0,\n"
					"in float4 uv1 : TEXCOORD1){\n"
					"float4 texcol = tex2D(samp0,uv0.xy);\n"
					"float4 EncodedDepth = frac((texcol.r * (16777215.0f/16777216.0f)) * float4(1.0f,255.0f,255.0f*255.0f,255.0f*255.0f*255.0f));\n"
					"texcol = float4((EncodedDepth.rgb * (16777216.0f/16777215.0f)),1.0f);\n"
					"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"						
					"}\n",C_COLORMATRIX);
	s_DepthMatrixProgram[1] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));

	//4 sample SSAA
	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"					
					"uniform float4 cColMatrix[5] : register(c%d);\n"
					"void main(\n"
					"out float4 ocol0 : COLOR0,\n"
					"in float4 uv0 : TEXCOORD0,\n"
					"in float4 uv1 : TEXCOORD1,\n"
					"in float4 uv2 : TEXCOORD2,\n"
					"in float4 uv3 : TEXCOORD3){\n"
					"float4 texcol = (tex2D(samp0,uv1.xy) + tex2D(samp0,uv1.wz) + tex2D(samp0,uv2.xy) + tex2D(samp0,uv2.wz))*0.25f;\n"
					"float4 EncodedDepth = frac((texcol.r * (16777215.0f/16777216.0f)) * float4(1.0f,255.0f,255.0f*255.0f,255.0f*255.0f*255.0f));\n"
					"texcol = float4((EncodedDepth.rgb * (16777216.0f/16777215.0f)),1.0f);\n"
					"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"						
					"}\n",C_COLORMATRIX);
	s_DepthMatrixProgram[2] = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	SETSTAT(stats.numPixelShadersCreated, 0);
	SETSTAT(stats.numPixelShadersAlive, 0);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx9-%s-ps.cache", File::GetUserPath(D_SHADERCACHE_IDX), g_globals->unique_id);
	PixelShaderCacheInserter inserter;
	int read_items = g_ps_disk_cache.OpenAndRead(cache_filename, &inserter);
}

// ONLY to be used during shutdown.
void PixelShaderCache::Clear()
{
	PSCache::iterator iter = PixelShaders.begin();
	for (; iter != PixelShaders.end(); ++iter)
		iter->second.Destroy();
	PixelShaders.clear(); 

	memset(&last_pixel_shader_uid, 0xFF, sizeof(last_pixel_shader_uid));
}

PixelShaderCache::~PixelShaderCache()
{
	for(int i = 0;i < MAX_SSAA_SHADERS; i++)
	{
		if (s_ColorMatrixProgram[i]) s_ColorMatrixProgram[i]->Release();
		s_ColorMatrixProgram[i] = NULL;
		if (s_ColorCopyProgram[i]) s_ColorCopyProgram[i]->Release();
		s_ColorCopyProgram[i] = NULL;
		if (s_DepthMatrixProgram[i]) s_DepthMatrixProgram[i]->Release();
		s_DepthMatrixProgram[i] = NULL;
	}
	if (s_ClearProgram)	s_ClearProgram->Release();
	s_ClearProgram = NULL;	
	
	Clear();
	g_ps_disk_cache.Sync();
	g_ps_disk_cache.Close();

	unique_shaders.clear();
}

bool PixelShaderCache::SetShader(bool dstAlpha)
{
	PIXELSHADERUID uid;
	GetPixelShaderId(&uid, dstAlpha);

	// Is the shader already set?
	if (uid == last_pixel_shader_uid && PixelShaders[uid].frameCount == frameCount)
	{
		PSCache::const_iterator iter = PixelShaders.find(uid);
		if (iter != PixelShaders.end() && iter->second.shader)
			return true;   // Sure, we're done.
		else
			return false;  // ?? something is wrong.
	}

	memcpy(&last_pixel_shader_uid, &uid, sizeof(PIXELSHADERUID));
	
	// Is the shader already in the cache?
	PSCache::iterator iter;
	iter = PixelShaders.find(uid);
	if (iter != PixelShaders.end())
	{
		iter->second.frameCount = frameCount;
		const PSCacheEntry &entry = iter->second;
		last_entry = &entry;

		if (entry.shader)
		{
			D3D::SetPixelShader(entry.shader);
			return true;
		}
		else
			return false;
	}

	// OK, need to generate and compile it.
	const char *code = GeneratePixelShaderCode(dstAlpha, API_D3D9);

	u32 code_hash = HashAdler32((const u8 *)code, strlen(code));
	unique_shaders.insert(code_hash);
	SETSTAT(stats.numUniquePixelShaders, unique_shaders.size());

	#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS && code) {	
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%sps_%04i.txt", File::GetUserPath(D_DUMP_IDX), counter++);
		
		SaveData(szTemp, code);
	}
	#endif

	u8 *bytecode = 0;
	int bytecodelen = 0;
	if (!D3D::CompilePixelShader(code, (int)strlen(code), &bytecode, &bytecodelen)) {
		if (g_ActiveConfig.bShowShaderErrors)
		{
			PanicAlert("Failed to compile Pixel Shader:\n\n%s", code);
			static int counter = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%sBADps_%04i.txt", File::GetUserPath(D_DUMP_IDX), counter++);			
			SaveData(szTemp, code);
		}
		return false;
	}

	// Here we have the UID and the byte code. Insert it into the disk cache.
	g_ps_disk_cache.Append((u8 *)&uid, sizeof(uid), bytecode, bytecodelen);
	g_ps_disk_cache.Sync();

	// And insert it into the shader cache.
	bool result = InsertByteCode(uid, bytecode, bytecodelen, true);
	delete [] bytecode;
	return result;
}

bool PixelShaderCache::InsertByteCode(const PIXELSHADERUID &uid, const u8 *bytecode, int bytecodelen, bool activate) {
	LPDIRECT3DPIXELSHADER9 shader = D3D::CreatePixelShaderFromByteCode(bytecode, bytecodelen);

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
	SETSTAT(stats.numPixelShadersAlive, (int)PixelShaders.size());
	if (activate)
	{
		D3D::SetPixelShader(shader);
	}
	return true;
}


#if defined(_DEBUG) || defined(DEBUGFAST)
std::string PixelShaderCache::GetCurrentShaderCode()
{
	if (last_entry)
		return last_entry->code;
	else
		return "(no shader)\n";
}
#endif

}
