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
#include "Config.h"
#include "VertexShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"

#include <Cg/cg.h>
#include <Cg/cgD3D9.h>

VertexShaderCache::VSCache VertexShaderCache::vshaders;
const VertexShaderCache::VSCacheEntry *VertexShaderCache::last_entry;

void SetVSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	const float f[4] = {f1, f2, f3, f4};
	D3D::dev->SetVertexShaderConstantF(const_number, f, 1);
}

void SetVSConstant4fv(int const_number, const float *f)
{
	D3D::dev->SetVertexShaderConstantF(const_number, f, 1);
}

void VertexShaderCache::Init()
{

}

void VertexShaderCache::Shutdown()
{
	VSCache::iterator iter = vshaders.begin();
	for (; iter != vshaders.end(); iter++)
		iter->second.Destroy();
	vshaders.clear();
}


void VertexShaderCache::SetShader(u32 components)
{
	static LPDIRECT3DVERTEXSHADER9 last_shader = NULL;

	DVSTARTPROFILE();

	VERTEXSHADERUID uid;
	GetVertexShaderId(uid, components);

	VSCache::iterator iter;
	iter = vshaders.find(uid);
	if (iter != vshaders.end())
	{
		iter->second.frameCount = frameCount;
		const VSCacheEntry &entry = iter->second;
		last_entry = &entry;
		if (!last_shader || entry.shader != last_shader)
		{
			D3D::dev->SetVertexShader(entry.shader);
			last_shader = entry.shader;
		}
		return;
	}

	bool HLSL = true;
	const char *code = GenerateVertexShader(components, HLSL);
	LPDIRECT3DVERTEXSHADER9 shader = HLSL ? D3D::CompileVertexShader(code, (int)strlen(code), false) : CompileCgShader(code);
	if (shader)
	{
		// Make an entry in the table
		VSCacheEntry entry;
		entry.shader = shader;
		entry.frameCount = frameCount;
#ifdef _DEBUG
		entry.code = code;
#endif
		vshaders[uid] = entry;
		last_entry = &vshaders[uid];
		last_shader = entry.shader;

		D3D::dev->SetVertexShader(shader);

		INCSTAT(stats.numVertexShadersCreated);
		SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
	}
	else if (g_Config.bShowShaderErrors)
	{
		PanicAlert("Failed to compile Vertex Shader:\n\n%s", code);
	}
	Renderer::SetFVF(NULL);
	D3D::dev->SetVertexShader(shader);
}

LPDIRECT3DVERTEXSHADER9 VertexShaderCache::CompileCgShader(const char *pstrprogram) 
{
	//char stropt[64];
	//sprintf(stropt, "MaxLocalParams=256,MaxInstructions=%d", s_nMaxVertexInstructions);
	const char *opts[] = {"-profileopts", "MaxLocalParams=256", "-O2", "-q", NULL};
	//const char **opts = cgD3D9GetOptimalOptions(g_cgvProf);
	CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgvProf, "main", opts);
	if (!cgIsProgram(tempprog) || cgGetError() != CG_NO_ERROR) {
		ERROR_LOG(VIDEO, "Failed to load vs %s:\n", cgGetLastListing(g_cgcontext));
		ERROR_LOG(VIDEO, pstrprogram);
		return NULL;
	}
	const char *pcompiledprog = cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);

	LPDIRECT3DVERTEXSHADER9 vertex_shader = D3D::CompileVertexShader(pcompiledprog, (int)strlen(pcompiledprog), true);
	cgDestroyProgram(tempprog);
	tempprog = NULL;
	return vertex_shader;
}

void VertexShaderCache::Cleanup()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end();)
	{
		VSCacheEntry &entry = iter->second;
		if (entry.frameCount < frameCount - 30)
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

#ifdef _DEBUG
std::string VertexShaderCache::GetCurrentShaderCode()
{
	if (last_entry)
		return last_entry->code;
	else
		return "(no shader)\n";
}
#endif