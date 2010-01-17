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
#include "D3DShader.h"
#include "Statistics.h"
#include "Utils.h"
#include "VideoConfig.h"
#include "PixelShaderGen.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"
#include "ImageWrite.h"

#include "Debugger/Debugger.h"

PixelShaderCache::PSCache PixelShaderCache::PixelShaders;
const PixelShaderCache::PSCacheEntry *PixelShaderCache::last_entry;

LinearDiskCache g_ps_disk_cache;

static float lastPSconstants[C_COLORMATRIX+16][4];

static LPDIRECT3DPIXELSHADER9 s_ColorMatrixProgram = 0;
static LPDIRECT3DPIXELSHADER9 s_ColorCopyProgram = 0;
static LPDIRECT3DPIXELSHADER9 s_ClearProgram = 0;
static LPDIRECT3DPIXELSHADER9 s_ClearZProgram = 0;
static LPDIRECT3DPIXELSHADER9 s_DepthMatrixProgram = 0;

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetColorMatrixProgram()
{
	return s_ColorMatrixProgram;
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetDepthMatrixProgram()
{
	return s_DepthMatrixProgram;
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetColorCopyProgram()
{
	return s_ColorCopyProgram;
}

LPDIRECT3DPIXELSHADER9 PixelShaderCache::GetClearProgram()
{
	return s_ClearProgram;
}

void SetPSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	if (lastPSconstants[const_number][0] != f1 || lastPSconstants[const_number][1] != f2 ||
		lastPSconstants[const_number][2] != f3 || lastPSconstants[const_number][3] != f4 )
	{
		const float f[4] = {f1, f2, f3, f4};
		D3D::dev->SetPixelShaderConstantF(const_number, f, 1);
		lastPSconstants[const_number][0] = f1;
		lastPSconstants[const_number][1] = f2;
		lastPSconstants[const_number][2] = f3;
		lastPSconstants[const_number][3] = f4;
	}
}

void SetPSConstant4fv(int const_number, const float *f)
{
	if (lastPSconstants[const_number][0] != f[0] || lastPSconstants[const_number][1] != f[1] ||
		lastPSconstants[const_number][2] != f[2] || lastPSconstants[const_number][3] != f[3] )
	{
		D3D::dev->SetPixelShaderConstantF(const_number, f, 1);
		lastPSconstants[const_number][0] = f[0];
		lastPSconstants[const_number][1] = f[1];
		lastPSconstants[const_number][2] = f[2];
		lastPSconstants[const_number][3] = f[3];
	}
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

void PixelShaderCache::Init()
{
	char pprog[1024];
	sprintf(pprog, "void main(\n"
						"out float4 ocol0 : COLOR0,\n"
						" in float4 incol0 : COLOR0){\n"
						"ocol0 = incol0;\n"
						"}\n");
	s_ClearProgram = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));	

	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
						"void main(\n"
						"out float4 ocol0 : COLOR0,\n"
						" in float3 uv0 : TEXCOORD0){\n"
						"ocol0 = tex2D(samp0,uv0.xy);\n"						
						"}\n");
	s_ColorCopyProgram = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));

	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
						"uniform float4 cColMatrix[5] : register(c%d);\n"
						"void main(\n"
						"out float4 ocol0 : COLOR0,\n"
						" in float3 uv0 : TEXCOORD0){\n"
						"float4 texcol = tex2D(samp0,uv0.xy);\n"
						"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"						
						"}\n",C_COLORMATRIX);
	s_ColorMatrixProgram = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));

	sprintf(pprog, "uniform sampler samp0 : register(s0);\n"
						"uniform float4 cColMatrix[5] : register(c%d);\n"
						"void main(\n"
						"out float4 ocol0 : COLOR0,\n"
						" in float3 uv0 : TEXCOORD0){\n"
						"float4 texcol = tex2D(samp0,uv0.xy);\n"
						"float4 EncodedDepth = frac((texcol.r * (16777215.0f/16777216.0f)) * float4(1.0f,255.0f,255.0f*255.0f,255.0f*255.0f*255.0f));\n"
						"texcol = float4((EncodedDepth.rgb * (16777216.0f/16777215.0f)),1.0f);\n"
						"ocol0 = float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + cColMatrix[4];\n"						
						"}\n",C_COLORMATRIX);
	s_DepthMatrixProgram = D3D::CompileAndCreatePixelShader(pprog, (int)strlen(pprog));	
	Clear();

	if (!File::Exists(FULL_SHADERCACHE_DIR))
		File::CreateDir(FULL_SHADERCACHE_DIR);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%s%s-ps.cache", FULL_SHADERCACHE_DIR, globals->unique_id);
	PixelShaderCacheInserter inserter;
	int read_items = g_ps_disk_cache.OpenAndRead(cache_filename, &inserter);
}

// ONLY to be used during shutdown.
void PixelShaderCache::Clear()
{
	PSCache::iterator iter = PixelShaders.begin();
	for (; iter != PixelShaders.end(); iter++)
		iter->second.Destroy();
	PixelShaders.clear(); 

	for (int i = 0; i < (C_COLORMATRIX + 16) * 4; i++)
		lastPSconstants[i / 4][i % 4] = -100000000.0f;
	memset(&last_pixel_shader_uid, 0xFF, sizeof(last_pixel_shader_uid));
}

void PixelShaderCache::Shutdown()
{
	if (s_ColorMatrixProgram) s_ColorMatrixProgram->Release();
	s_ColorMatrixProgram = NULL;
	if (s_ColorCopyProgram) s_ColorCopyProgram->Release();
	s_ColorCopyProgram = NULL;
	if (s_DepthMatrixProgram) s_DepthMatrixProgram->Release();
	s_DepthMatrixProgram = NULL;
	if (s_ClearProgram)	s_ClearProgram->Release();
	s_ClearProgram = NULL;	
	
	Clear();
	g_ps_disk_cache.Sync();
	g_ps_disk_cache.Close();
}

bool PixelShaderCache::SetShader(bool dstAlpha)
{
	PIXELSHADERUID uid;
	GetPixelShaderId(&uid, PixelShaderManager::GetTextureMask(), dstAlpha);

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
		
		DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE,true);

		if (entry.shader)
		{
			D3D::SetPixelShader(entry.shader);
			return true;
		}
		else
			return false;
	}

	// OK, need to generate and compile it.
	const char *code = GeneratePixelShaderCode(PixelShaderManager::GetTextureMask(), dstAlpha, 2);
	#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS && code) {	
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%s/ps_%04i.txt", FULL_DUMP_DIR, counter++);
		
		SaveData(szTemp, code);
	}
	#endif

	u8 *bytecode = 0;
	int bytecodelen = 0;
	if (!D3D::CompilePixelShader(code, (int)strlen(code), &bytecode, &bytecodelen)) {
		if (g_ActiveConfig.bShowShaderErrors)
		{
			PanicAlert("Failed to compile Pixel Shader:\n\n%s", code);
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