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

#include "Common.h"
#include "FileUtil.h"
#include "LinearDiskCache.h"

#include "Globals.h"
#include "D3DBase.h"
#include "D3DShader.h"
#include "Statistics.h"
#include "VideoConfig.h"
#include "VertexShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"
#include "Debugger.h"
#include "ConfigManager.h"

namespace DX9
{

VertexShaderCache::VSCache VertexShaderCache::vshaders;
const VertexShaderCache::VSCacheEntry *VertexShaderCache::last_entry;
VERTEXSHADERUID VertexShaderCache::last_uid;

#define MAX_SSAA_SHADERS 3

static LPDIRECT3DVERTEXSHADER9 SimpleVertexShader[MAX_SSAA_SHADERS];
static LPDIRECT3DVERTEXSHADER9 ClearVertexShader;

LinearDiskCache<VERTEXSHADERUID, u8> g_vs_disk_cache;

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetSimpleVertexShader(int level)
{
	return SimpleVertexShader[level % MAX_SSAA_SHADERS];
}

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetClearVertexShader()
{
	return ClearVertexShader;
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader<VERTEXSHADERUID, u8>
{
public:
	void Read(const VERTEXSHADERUID &key, const u8 *value, u32 value_size)
	{
		VertexShaderCache::InsertByteCode(key, value, value_size, false);
	}
};

void VertexShaderCache::Init()
{
	char* vProg = new char[2048];
	sprintf(vProg,"struct VSOUTPUT\n"
						"{\n"
							"float4 vPosition : POSITION;\n"
							"float2 vTexCoord : TEXCOORD0;\n"
							"float vTexCoord1 : TEXCOORD1;\n"
						"};\n"
						"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0,float2 inTEX1 : TEXCOORD1,float inTEX2 : TEXCOORD2)\n"
						"{\n"
							"VSOUTPUT OUT;\n"
							"OUT.vPosition = inPosition;\n"
							"OUT.vTexCoord = inTEX0;\n"
							"OUT.vTexCoord1 = inTEX2;\n"
							"return OUT;\n"
						"}\n");

	SimpleVertexShader[0] = D3D::CompileAndCreateVertexShader(vProg, (int)strlen(vProg));

	sprintf(vProg,"struct VSOUTPUT\n"
						"{\n"
							"float4 vPosition   : POSITION;\n"
							"float4 vColor0   : COLOR0;\n"
						"};\n"
						"VSOUTPUT main(float4 inPosition : POSITION,float4 inColor0: COLOR0)\n"
						"{\n"
							"VSOUTPUT OUT;\n"
							"OUT.vPosition = inPosition;\n"
							"OUT.vColor0 = inColor0;\n"
							"return OUT;\n"
						"}\n");

	ClearVertexShader = D3D::CompileAndCreateVertexShader(vProg, (int)strlen(vProg));
	sprintf(vProg,	"struct VSOUTPUT\n"
						"{\n"
							"float4 vPosition   : POSITION;\n"
							"float2 vTexCoord   : TEXCOORD0;\n"
							"float vTexCoord1   : TEXCOORD1;\n"
						"};\n"
						"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0,float2 inInvTexSize : TEXCOORD1,float inTEX2 : TEXCOORD2)\n"
						"{\n"
							"VSOUTPUT OUT;"
							"OUT.vPosition = inPosition;\n"
							// HACK: Scale the texture coordinate range from (0,width) to (0,width-1), otherwise the linear filter won't average our samples correctly
							"OUT.vTexCoord  = inTEX0 * (float2(1.f,1.f) / inInvTexSize - float2(1.f,1.f)) * inInvTexSize;\n"
							"OUT.vTexCoord1 = inTEX2;\n"
							"return OUT;\n"
						"}\n");
	SimpleVertexShader[1] = D3D::CompileAndCreateVertexShader(vProg, (int)strlen(vProg));

	sprintf(vProg,	"struct VSOUTPUT\n"
						"{\n"
							"float4 vPosition   : POSITION;\n"
							"float4 vTexCoord   : TEXCOORD0;\n"
							"float  vTexCoord1   : TEXCOORD1;\n"
							"float4 vTexCoord2   : TEXCOORD2;\n"   
							"float4 vTexCoord3   : TEXCOORD3;\n"
						"};\n"
						"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0,float2 inTEX1 : TEXCOORD1,float inTEX2 : TEXCOORD2)\n"
						"{\n"
							"VSOUTPUT OUT;"
							"OUT.vPosition = inPosition;\n"
							"OUT.vTexCoord  = inTEX0.xyyx;\n"
							"OUT.vTexCoord1 = inTEX2.x;\n"
							"OUT.vTexCoord2 = inTEX0.xyyx + (float4(-1.0f,-0.5f, 1.0f,-0.5f) * inTEX1.xyyx);\n"
							"OUT.vTexCoord3 = inTEX0.xyyx + (float4( 1.0f, 0.5f,-1.0f, 0.5f) * inTEX1.xyyx);\n"	
							"return OUT;\n"
						"}\n");
	SimpleVertexShader[2] = D3D::CompileAndCreateVertexShader(vProg, (int)strlen(vProg));	
	
	Clear();
	delete [] vProg;

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());

	SETSTAT(stats.numVertexShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, 0);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx9-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());
	VertexShaderCacheInserter inserter;
	g_vs_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_Config.bEnableShaderDebugging)
		Clear();

	last_entry = NULL;
}

void VertexShaderCache::Clear()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); ++iter)
		iter->second.Destroy();
	vshaders.clear();

	last_entry = NULL;
}

void VertexShaderCache::Shutdown()
{
	for (int i = 0; i < MAX_SSAA_SHADERS; i++)
	{
		if (SimpleVertexShader[i])
			SimpleVertexShader[i]->Release();
		SimpleVertexShader[i] = NULL;
	}

	if (ClearVertexShader)
		ClearVertexShader->Release();
	ClearVertexShader = NULL;
	
	Clear();
	g_vs_disk_cache.Sync();
	g_vs_disk_cache.Close();
}

bool VertexShaderCache::SetShader(u32 components)
{
	VERTEXSHADERUID uid;
	GetVertexShaderId(&uid, components);
	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
			ValidateVertexShaderIDs(API_D3D9, last_entry->safe_uid, last_entry->code, components);
			return (last_entry->shader != NULL);
		}
	}

	last_uid = uid;

	VSCache::iterator iter = vshaders.find(uid);
	if (iter != vshaders.end())
	{
		const VSCacheEntry &entry = iter->second;
		last_entry = &entry;

		if (entry.shader) D3D::SetVertexShader(entry.shader);
		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
		ValidateVertexShaderIDs(API_D3D9, entry.safe_uid, entry.code, components);
		return (entry.shader != NULL);
	}

	const char *code = GenerateVertexShaderCode(components, API_D3D9);
	u8 *bytecode;
	int bytecodelen;
	if (!D3D::CompileVertexShader(code, (int)strlen(code), &bytecode, &bytecodelen))
	{
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return false;
	}
	g_vs_disk_cache.Append(uid, bytecode, bytecodelen);

	bool success = InsertByteCode(uid, bytecode, bytecodelen, true);
	if (g_ActiveConfig.bEnableShaderDebugging && success)
	{
		vshaders[uid].code = code;
		GetSafeVertexShaderId(&vshaders[uid].safe_uid, components);
	}
	delete [] bytecode;
	GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	return success;
}

bool VertexShaderCache::InsertByteCode(const VERTEXSHADERUID &uid, const u8 *bytecode, int bytecodelen, bool activate) {
	LPDIRECT3DVERTEXSHADER9 shader = D3D::CreateVertexShaderFromByteCode(bytecode, bytecodelen);

	// Make an entry in the table
	VSCacheEntry entry;
	entry.shader = shader;

	vshaders[uid] = entry;
	last_entry = &vshaders[uid];
	if (!shader)
		return false;

	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
	if (activate)
	{
		D3D::SetVertexShader(shader);
		return true;
	}
	return false;
}

void Renderer::SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	const float f[4] = { f1, f2, f3, f4 };
	DX9::D3D::dev->SetVertexShaderConstantF(const_number, f, 1);
}

void Renderer::SetVSConstant4fv(unsigned int const_number, const float *f)
{
	DX9::D3D::dev->SetVertexShaderConstantF(const_number, f, 1);
}

void Renderer::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
{
	float buf[4*C_VENVCONST_END];
	for (unsigned int i = 0; i < count; i++)
	{
		buf[4*i  ] = *f++;
		buf[4*i+1] = *f++;
		buf[4*i+2] = *f++;
		buf[4*i+3] = 0.f;
	}
	DX9::D3D::dev->SetVertexShaderConstantF(const_number, buf, count);
}

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	DX9::D3D::dev->SetVertexShaderConstantF(const_number, f, count);
}

}  // namespace DX9
