// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
VertexShaderUid VertexShaderCache::last_uid;
UidChecker<VertexShaderUid,VertexShaderCode> VertexShaderCache::vertex_uid_checker;

#define MAX_SSAA_SHADERS 3

static LPDIRECT3DVERTEXSHADER9 SimpleVertexShader[MAX_SSAA_SHADERS];
static LPDIRECT3DVERTEXSHADER9 ClearVertexShader;

LinearDiskCache<VertexShaderUid, u8> g_vs_disk_cache;

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetSimpleVertexShader(int level)
{
	return SimpleVertexShader[level % MAX_SSAA_SHADERS];
}

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetClearVertexShader()
{
	return ClearVertexShader;
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader<VertexShaderUid, u8>
{
public:
	void Read(const VertexShaderUid &key, const u8 *value, u32 value_size)
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
							"OUT.vTexCoord  = inTEX0;\n"
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
	vertex_uid_checker.Invalidate();

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
	VertexShaderUid uid;
	GetVertexShaderUid(uid, components, API_D3D9);
	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		VertexShaderCode code;
		GenerateVertexShaderCode(code, components, API_D3D9);
		vertex_uid_checker.AddToIndexAndCheck(code, uid, "Vertex", "v");
	}

	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
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
		return (entry.shader != NULL);
	}

	VertexShaderCode code;
	GenerateVertexShaderCode(code, components, API_D3D9);

	u8 *bytecode;
	int bytecodelen;
	if (!D3D::CompileVertexShader(code.GetBuffer(), (int)strlen(code.GetBuffer()), &bytecode, &bytecodelen))
	{
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return false;
	}
	g_vs_disk_cache.Append(uid, bytecode, bytecodelen);

	bool success = InsertByteCode(uid, bytecode, bytecodelen, true);
	if (g_ActiveConfig.bEnableShaderDebugging && success)
	{
		vshaders[uid].code = code.GetBuffer();
	}
	delete [] bytecode;
	GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	return success;
}

bool VertexShaderCache::InsertByteCode(const VertexShaderUid &uid, const u8 *bytecode, int bytecodelen, bool activate) {
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
