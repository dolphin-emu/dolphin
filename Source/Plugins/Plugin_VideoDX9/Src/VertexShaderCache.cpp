// Copyright (C) 2003-2008 Dolphin Project.

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
#include "VertexShaderCache.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"

#include <Cg/cg.h>
#include <Cg/cgD3D9.h>

VertexShaderCache::VSCache VertexShaderCache::vshaders;

void SetVSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	//const float f[4] = {f1, f2, f3, f4};
	//D3D::dev->SetVertexShaderConstantF(const_number, f, 1);
	

	// TODO: The Cg Way
	/** CGparameter param = cgGetNamedParameter(program, "someParameter"); 
	cgSetParameter4f(param, f1, f2, f3, f4); **/
}

void SetVSConstant4fv(int const_number, const float *f)
{
	//D3D::dev->SetVertexShaderConstantF(const_number, f, 1);


	// TODO: The Cg Way
	/** CGparameter param = cgGetNamedParameter(program, "someParameter"); 
	cgSetParameter4fv(param, f); **/
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
	static CGprogram shader = NULL;
	if (D3D::GetShaderVersion() < 2)
		return; // we are screwed

	static CGprogram lastShader = NULL;
	DVSTARTPROFILE();

	VERTEXSHADERUID uid;
	GetVertexShaderId(uid, components, false);

	VSCache::iterator iter;
	iter = vshaders.find(uid);

	if (iter != vshaders.end())
	{
		iter->second.frameCount = frameCount;
		VSCacheEntry &entry = iter->second;
		if (!lastShader || entry.shader != lastShader)
		{
			//D3D::dev->SetVertexShader(entry.shader);
			if(!cgD3D9IsProgramLoaded(entry.shader))
				cgD3D9LoadProgram(entry.shader, false, 0);
			cgD3D9BindProgram(entry.shader);
			lastShader = entry.shader;
		}
		return;
	}

	const char *code = GenerateVertexShader(components, false);
	//shader = D3D::CompileVertexShader(code, (int)strlen(code));
	shader = CompileCgShader(code);
	if (shader)
	{
		//Make an entry in the table
		VSCacheEntry entry;
		entry.shader = shader;
		entry.frameCount = frameCount;
		vshaders[uid] = entry;

		// There seems to be an unknown Cg error here for some reason
		///PanicAlert("Load vShader");
		if(!cgD3D9IsProgramLoaded(shader))
			cgD3D9LoadProgram(shader, false, 0);
		cgD3D9BindProgram(shader);
		D3D::dev->SetFVF(NULL);
		///PanicAlert("Loaded vShader");

		INCSTAT(stats.numVertexShadersCreated);
		SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());
	} else
		PanicAlert("Failed to compile Vertex Shader:\n\n%s", code);

	//D3D::dev->SetVertexShader(shader);
}

CGprogram VertexShaderCache::CompileCgShader(const char *pstrprogram) 
{
	char stropt[64];
	//sprintf(stropt, "MaxLocalParams=256,MaxInstructions=%d", s_nMaxVertexInstructions);
	//const char *opts[] = {"-profileopts", stropt, "-O2", "-q", NULL};
	const char **opts = cgD3D9GetOptimalOptions(g_cgvProf);
	CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgvProf, "main", opts);
	if (!cgIsProgram(tempprog) || cgGetError() != CG_NO_ERROR) {
		ERROR_LOG(VIDEO, "Failed to load vs %s:\n", cgGetLastListing(g_cgcontext));
		ERROR_LOG(VIDEO, pstrprogram);
		return NULL;
	}

	// This looks evil - we modify the program through the const char * we got from cgGetProgramString!
	// It SHOULD not have any nasty side effects though - but you never know...
	char *pcompiledprog = (char*)cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);
	char *plocal = strstr(pcompiledprog, "program.local");
	while (plocal != NULL) {
		const char* penv = "  program.env";
		memcpy(plocal, penv, 13);
		plocal = strstr(plocal + 13, "program.local");
	}

	return tempprog;
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