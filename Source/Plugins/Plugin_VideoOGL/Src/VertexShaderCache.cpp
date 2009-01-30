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

#include <math.h>

#include "Globals.h"
#include "Profiler.h"
#include "Config.h"
#include "Statistics.h"

#include "GLUtil.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

#include "Render.h"
#include "VertexShaderGen.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexManager.h"
#include "VertexLoader.h"
#include "XFMemory.h"
#include "ImageWrite.h"

VertexShaderCache::VSCache VertexShaderCache::vshaders;

static VERTEXSHADER *pShaderLast = NULL;
static int s_nMaxVertexInstructions;

void SetVSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, const_number, f1, f2, f3, f4);
}

void SetVSConstant4fv(int const_number, const float *f)
{
	glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number, f);
}


void VertexShaderCache::Init()
{
	glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, (GLint *)&s_nMaxVertexInstructions);
}

void VertexShaderCache::Shutdown()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); iter++)
		iter->second.Destroy();
	vshaders.clear();
}


VERTEXSHADER* VertexShaderCache::GetShader(u32 components)
{
	DVSTARTPROFILE();
	VERTEXSHADERUID uid;
	u32 zbufrender = (bpmem.ztex2.op == ZTEXTURE_ADD) || Renderer::GetZBufferTarget() != 0;
	GetVertexShaderId(uid, components, zbufrender);

	VSCache::iterator iter = vshaders.find(uid);

	if (iter != vshaders.end()) {
		iter->second.frameCount = frameCount;
		VSCacheEntry &entry = iter->second;
		if (&entry.shader != pShaderLast) {
			pShaderLast = &entry.shader;
		}
		return pShaderLast;
	}

	VSCacheEntry& entry = vshaders[uid];
	const char *code = GenerateVertexShader(components, Renderer::GetZBufferTarget() != 0);

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_Config.iLog & CONF_SAVESHADERS && code) {
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%s/vs_%04i.txt", g_Config.texDumpPath, counter++);

		SaveData(szTemp, code);
	}
#endif

	if (!code || !VertexShaderCache::CompileVertexShader(entry.shader, code)) {
		ERROR_LOG("failed to create vertex shader\n");
		return NULL;
	}

	//Make an entry in the table
	entry.frameCount = frameCount;
	pShaderLast = &entry.shader;
	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, vshaders.size());
	return pShaderLast;
}

void VertexShaderCache::Cleanup()
{
	VSCache::iterator iter = vshaders.begin();
	while (iter != vshaders.end()) {
		VSCacheEntry &entry = iter->second;
		if (entry.frameCount < frameCount - 200) {
			entry.Destroy();
#ifdef _WIN32
			iter = vshaders.erase(iter);
#else
			vshaders.erase(iter++);
#endif
		}
		else {
			++iter;
		}
	}

	SETSTAT(stats.numVertexShadersAlive, vshaders.size());
}

bool VertexShaderCache::CompileVertexShader(VERTEXSHADER& vs, const char* pstrprogram)
{
	char stropt[64];
	sprintf(stropt, "MaxLocalParams=256,MaxInstructions=%d", s_nMaxVertexInstructions);
	const char *opts[] = {"-profileopts", stropt, "-O2", "-q", NULL};
	CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgvProf, "main", opts);
	if (!cgIsProgram(tempprog) || cgGetError() != CG_NO_ERROR) {
		ERROR_LOG("Failed to load vs %s:\n", cgGetLastListing(g_cgcontext));
		ERROR_LOG(pstrprogram);
		return false;
	}

	//ERROR_LOG(pstrprogram);
	//ERROR_LOG("id: %d\n", g_Config.iSaveTargetId);

	char* pcompiledprog = (char*)cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);
	char* plocal = strstr(pcompiledprog, "program.local");

	while (plocal != NULL) {
		const char* penv = "  program.env";
		memcpy(plocal, penv, 13);
		plocal = strstr(plocal+13, "program.local");
	}

	glGenProgramsARB(1, &vs.glprogid);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vs.glprogid);
	glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pcompiledprog), pcompiledprog);

	GLenum err = GL_NO_ERROR;
	GL_REPORT_ERROR();
	if (err != GL_NO_ERROR) {
		ERROR_LOG(pstrprogram);
		ERROR_LOG(pcompiledprog);
	}

	cgDestroyProgram(tempprog);
	// printf("Compiled vertex shader %i\n", vs.glprogid);

#if defined(_DEBUG) || defined(DEBUGFAST) 
	vs.strprog = pstrprogram;
#endif

	return true;
}
