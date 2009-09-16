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

static float GC_ALIGNED16(lastVSconstants[C_FOGPARAMS+8][4]);

void SetVSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	if (lastVSconstants[const_number][0] != f1 || 
		lastVSconstants[const_number][1] != f2 ||
		lastVSconstants[const_number][2] != f3 ||
		lastVSconstants[const_number][3] != f4)
	{
		const float f[4] = {f1, f2, f3, f4};
		D3D::dev->SetVertexShaderConstantF(const_number, f, 1);
		lastVSconstants[const_number][0] = f1;
		lastVSconstants[const_number][1] = f2;
		lastVSconstants[const_number][2] = f3;
		lastVSconstants[const_number][3] = f4;
	}
}

void SetVSConstant4fv(int const_number, const float *f)
{
	if (lastVSconstants[const_number][0] != f[0] || 
		lastVSconstants[const_number][1] != f[1] ||
		lastVSconstants[const_number][2] != f[2] ||
		lastVSconstants[const_number][3] != f[3])
	{
		D3D::dev->SetVertexShaderConstantF(const_number, f, 1);
		lastVSconstants[const_number][0] = f[0];
		lastVSconstants[const_number][1] = f[1];
		lastVSconstants[const_number][2] = f[2];
		lastVSconstants[const_number][3] = f[3];
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
		D3D::dev->SetVertexShaderConstantF(const_number, f, count);
		for (int i = 0; i < count; i++) {
			lastVSconstants[const_number + i][0] = f[0 + i*4];
			lastVSconstants[const_number + i][1] = f[1 + i*4];
			lastVSconstants[const_number + i][2] = f[2 + i*4];
			lastVSconstants[const_number + i][3] = f[3 + i*4];
		}
	}
}

void VertexShaderCache::Init()
{
	Clear();
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
	Clear();
}

bool VertexShaderCache::SetShader(u32 components)
{
	DVSTARTPROFILE();

	VERTEXSHADERUID uid;
	GetVertexShaderId(uid, components);
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

		DEBUGGER_PAUSE_COUNT_N(NEXT_VERTEX_SHADER_CHANGE);
		if (entry.shader)
		{
			D3D::dev->SetVertexShader(entry.shader);
			return true;
		}
		else
			return false;
	}

	const char *code = GenerateVertexShader(components, true);
	LPDIRECT3DVERTEXSHADER9 shader = D3D::CompileVertexShader(code, (int)strlen(code));

	// Make an entry in the table
	VSCacheEntry entry;
	entry.shader = shader;
	entry.frameCount = frameCount;
#if defined(_DEBUG) || defined(DEBUGFAST)
	entry.code = code;
#endif
	vshaders[uid] = entry;
	last_entry = &vshaders[uid];
	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
	if (shader)
	{
		D3D::dev->SetVertexShader(shader);
		return true;
	}
	
	if (g_ActiveConfig.bShowShaderErrors)
	{
		PanicAlert("Failed to compile Vertex Shader:\n\n%s", code);
	}
	return false;
}

void VertexShaderCache::Cleanup()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end();)
	{
		VSCacheEntry &entry = iter->second;
		if (entry.frameCount < frameCount - 1400)
		{
			entry.Destroy();
			iter = vshaders.erase(iter);
		}
		else
		{
			++iter;
		}
	}
	SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
}

#if defined(_DEBUG) || defined(DEBUGFAST)
std::string VertexShaderCache::GetCurrentShaderCode()
{
	if (last_entry)
		return last_entry->code;
	else
		return "(no shader)\n";
}
#endif