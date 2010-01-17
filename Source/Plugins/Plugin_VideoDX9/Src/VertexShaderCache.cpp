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
#include "Utils.h"
#include "Profiler.h"
#include "VideoConfig.h"
#include "VertexShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"

#include "debugger/debugger.h"

VertexShaderCache::VSCache VertexShaderCache::vshaders;
const VertexShaderCache::VSCacheEntry *VertexShaderCache::last_entry;

static float GC_ALIGNED16(lastVSconstants[C_FOGPARAMS + 8][4]);

static LPDIRECT3DVERTEXSHADER9 SimpleVertexShader;
LinearDiskCache g_vs_disk_cache;

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::GetSimpleVertexShader()
{
	return SimpleVertexShader;
}

void SetVSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	if (lastVSconstants[const_number][0] != f1 || 
		lastVSconstants[const_number][1] != f2 ||
		lastVSconstants[const_number][2] != f3 ||
		lastVSconstants[const_number][3] != f4)
	{
		const float f[4] = {f1, f2, f3, f4};
		lastVSconstants[const_number][0] = f1;
		lastVSconstants[const_number][1] = f2;
		lastVSconstants[const_number][2] = f3;
		lastVSconstants[const_number][3] = f4;
		D3D::dev->SetVertexShaderConstantF(const_number, lastVSconstants[const_number], 1);
	}
}

void SetVSConstant4fv(int const_number, const float *f)
{
	if (lastVSconstants[const_number][0] != f[0] || 
		lastVSconstants[const_number][1] != f[1] ||
		lastVSconstants[const_number][2] != f[2] ||
		lastVSconstants[const_number][3] != f[3])
	{
		lastVSconstants[const_number][0] = f[0];
		lastVSconstants[const_number][1] = f[1];
		lastVSconstants[const_number][2] = f[2];
		lastVSconstants[const_number][3] = f[3];
		D3D::dev->SetVertexShaderConstantF(const_number, lastVSconstants[const_number], 1);
	}
}

void SetMultiVSConstant3fv(int const_number, int count, const float *f)
{
	bool change = false;
	for (int i = 0; i < count; i++)
	{
		if (lastVSconstants[const_number + i][0] != f[0 + i*3] || 
			lastVSconstants[const_number + i][1] != f[1 + i*3] ||
			lastVSconstants[const_number + i][2] != f[2 + i*3])
		{
			change = true;
			break;
		}
	}
	if (change)
	{
		for (int i = 0; i < count; i++)
		{
			lastVSconstants[const_number + i][0] = f[0 + i*3];
			lastVSconstants[const_number + i][1] = f[1 + i*3];
			lastVSconstants[const_number + i][2] = f[2 + i*3];
			lastVSconstants[const_number + i][3] = 0.0f;
		}
		D3D::dev->SetVertexShaderConstantF(const_number, lastVSconstants[const_number], count);
	}
}

void SetMultiVSConstant4fv(int const_number, int count, const float *f)
{
	bool change = false;
	for (int i = 0; i < count; i++)
	{
		if (lastVSconstants[const_number + i][0] != f[0 + i*4] || 
			lastVSconstants[const_number + i][1] != f[1 + i*4] ||
			lastVSconstants[const_number + i][2] != f[2 + i*4] ||
			lastVSconstants[const_number + i][3] != f[3 + i*4])
		{
			change = true;
			break;
		}
	}
	if (change)
	{
		for (int i = 0; i < count; i++)
		{
			lastVSconstants[const_number + i][0] = f[0 + i*4];
			lastVSconstants[const_number + i][1] = f[1 + i*4];
			lastVSconstants[const_number + i][2] = f[2 + i*4];
			lastVSconstants[const_number + i][3] = f[3 + i*4];
		}
		D3D::dev->SetVertexShaderConstantF(const_number, lastVSconstants[const_number], count);
	}
}

class VertexShaderCacheInserter : public LinearDiskCacheReader {
public:
	void Read(const u8 *key, int key_size, const u8 *value, int value_size)
	{
		VERTEXSHADERUID uid;
		if (key_size != sizeof(uid)) {
			ERROR_LOG(VIDEO, "Wrong key size in vertex shader cache");
			return;
		}
		memcpy(&uid, key, key_size);
		VertexShaderCache::InsertByteCode(uid, value, value_size, false);
	}
};

void VertexShaderCache::Init()
{
	char *vSimpleProg = new char[2048];
	sprintf(vSimpleProg,"struct VSOUTPUT\n"
						"{\n"
						   "float4 vPosition   : POSITION;\n"
						   "float4 Color    : COLOR0;\n"
						   "float4 vTexCoord   : TEXCOORD0;\n"
						   "float4 vTexCoord1   : TEXCOORD1;\n"
						"};\n"
						"VSOUTPUT main( float4 inPosition : POSITION, float4 inUV : TEXCOORD0,float4 inColor : COLOR0)\n"
						"{\n"
						   "VSOUTPUT OUT = (VSOUTPUT)0;\n"
						   "OUT.vPosition = inPosition;\n"
						   "OUT.Color = inColor;\n"
						   "OUT.vTexCoord = inUV;\n"
						   "OUT.vTexCoord1 = inPosition.zzzz;\n"
						   "return OUT;\n"
						"}\n");

	SimpleVertexShader = D3D::CompileAndCreateVertexShader(vSimpleProg, (int)strlen(vSimpleProg));
	Clear();
	delete [] vSimpleProg;

	if (!File::Exists(FULL_SHADERCACHE_DIR))
		File::CreateDir(FULL_SHADERCACHE_DIR);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%s%s-vs.cache", FULL_SHADERCACHE_DIR, globals->unique_id);
	VertexShaderCacheInserter inserter;
	int read_items = g_vs_disk_cache.OpenAndRead(cache_filename, &inserter);
}

void VertexShaderCache::Clear()
{
	VSCache::iterator iter = vshaders.begin();
	for (; iter != vshaders.end(); ++iter)
		iter->second.Destroy();
	vshaders.clear();

	for (int i = 0; i < (C_FOGPARAMS + 8) * 4; i++)
		lastVSconstants[i / 4][i % 4] = -100000000.0f;
	memset(&last_vertex_shader_uid, 0xFF, sizeof(last_vertex_shader_uid));
}

void VertexShaderCache::Shutdown()
{
	if (SimpleVertexShader)
		SimpleVertexShader->Release();
	Clear();
	g_vs_disk_cache.Sync();
	g_vs_disk_cache.Close();
}

bool VertexShaderCache::SetShader(u32 components)
{
	DVSTARTPROFILE();

	VERTEXSHADERUID uid;
	GetVertexShaderId(&uid, components);
	if (uid == last_vertex_shader_uid && vshaders[uid].frameCount == frameCount)
	{
		if (vshaders[uid].shader)
			return true;
		else
			return false;
	}
	memcpy(&last_vertex_shader_uid, &uid, sizeof(VERTEXSHADERUID));

	VSCache::iterator iter;
	iter = vshaders.find(uid);
	if (iter != vshaders.end())
	{
		iter->second.frameCount = frameCount;
		const VSCacheEntry &entry = iter->second;
		last_entry = &entry;

		DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE,true);
		if (entry.shader)
		{
			D3D::SetVertexShader(entry.shader);
			return true;
		}
		else
			return false;
	}

	const char *code = GenerateVertexShaderCode(components, true);
	u8 *bytecode;
	int bytecodelen;
	if (!D3D::CompileVertexShader(code, (int)strlen(code), &bytecode, &bytecodelen))
	{
		if (g_ActiveConfig.bShowShaderErrors)
		{
			PanicAlert("Failed to compile Vertex Shader:\n\n%s", code);
		}
		return false;
	}
	g_vs_disk_cache.Append((u8 *)&uid, sizeof(uid), bytecode, bytecodelen);
	g_vs_disk_cache.Sync();

	bool result = InsertByteCode(uid, bytecode, bytecodelen, true);
	delete [] bytecode;
	return result;
}

bool VertexShaderCache::InsertByteCode(const VERTEXSHADERUID &uid, const u8 *bytecode, int bytecodelen, bool activate) {
	LPDIRECT3DVERTEXSHADER9 shader = D3D::CreateVertexShaderFromByteCode(bytecode, bytecodelen);

	// Make an entry in the table
	VSCacheEntry entry;
	entry.shader = shader;
	entry.frameCount = frameCount;

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

#if defined(_DEBUG) || defined(DEBUGFAST)
std::string VertexShaderCache::GetCurrentShaderCode()
{
	return "(N/A)\n";
}
#endif