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

#include <math.h>

// Common
#include "FileUtil.h"

// VideoCommon
#include "Profiler.h"
#include "VideoConfig.h"
#include "Statistics.h"
#include "VertexShaderGen.h"
#include "VertexShaderManager.h"
#include "VertexLoader.h"
#include "XFMemory.h"
#include "ImageWrite.h"

// OGL
#include "OGL_Render.h"
#include "OGL_VertexShaderCache.h"
#include "OGL_GLUtil.h"
#include "OGL_VertexManager.h"

#include "../Main.h"

namespace OGL
{

VertexShaderCache::VSCache VertexShaderCache::vshaders;
bool VertexShaderCache::s_displayCompileAlert;
GLuint VertexShaderCache::CurrentShader;
bool VertexShaderCache::ShaderEnabled;

static VERTEXSHADER *pShaderLast = NULL;
static int s_nMaxVertexInstructions;

void VertexShaderCache::SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	float f[4] = { f1, f2, f3, f4 };
	glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number, f);
}

void VertexShaderCache::SetVSConstant4fv(unsigned int const_number, const float *f)
{
	glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number, f);
}

void VertexShaderCache::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	for (unsigned int i = 0; i < count; i++,f+=4)
		glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number + i, f);
}

void VertexShaderCache::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
{
	for (unsigned int i = 0; i < count; i++)
	{
		float buf[4];
		buf[0] = *f++;
		buf[1] = *f++;
		buf[2] = *f++;
		buf[3] = 0.f;
		glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number + i, buf);
	}
}


VertexShaderCache::VertexShaderCache()
{
	glEnable(GL_VERTEX_PROGRAM_ARB);
	ShaderEnabled = true;
	CurrentShader = 0;
	memset(&last_vertex_shader_uid, 0xFF, sizeof(last_vertex_shader_uid));

	s_displayCompileAlert = true;

	glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, (GLint *)&s_nMaxVertexInstructions);		
#if CG_VERSION_NUM == 2100
	if (strstr((const char*)glGetString(GL_VENDOR), "ATI") != NULL)
	{
		s_nMaxVertexInstructions = 4096;
	}
#endif
}

VertexShaderCache::~VertexShaderCache()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); iter++)
		iter->second.Destroy();
	vshaders.clear();
}

bool VertexShaderCache::SetShader(u32 components)
{
	const VERTEXSHADER* const vs = GetShader(components);
	if (vs)
	{
		SetCurrentShader(vs->glprogid);
		return true;
	}
	else
		return false;
}

VERTEXSHADER* VertexShaderCache::GetShader(u32 components)
{
	DVSTARTPROFILE();
	VERTEXSHADERUID uid;
	GetVertexShaderId(&uid, components);

	if (uid == last_vertex_shader_uid && vshaders[uid].frameCount == frameCount)
	{
		return pShaderLast;
	}
	memcpy(&last_vertex_shader_uid, &uid, sizeof(VERTEXSHADERUID));

	VSCache::iterator iter = vshaders.find(uid);

	if (iter != vshaders.end()) {
		iter->second.frameCount = frameCount;
		VSCacheEntry &entry = iter->second;
		if (&entry.shader != pShaderLast) {
			pShaderLast = &entry.shader;
		}

		return pShaderLast;
	}

	//Make an entry in the table
	VSCacheEntry& entry = vshaders[uid];
	entry.frameCount = frameCount;
	pShaderLast = &entry.shader;
	const char *code = GenerateVertexShaderCode(components, API_OPENGL);

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS && code) {
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%svs_%04i.txt", File::GetUserPath(D_DUMP_IDX), counter++);

		SaveData(szTemp, code);
	}
#endif

	if (!code || !VertexShaderCache::CompileVertexShader(entry.shader, code)) {
		ERROR_LOG(VIDEO, "failed to create vertex shader");
		return NULL;
	}

	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, vshaders.size());
	return pShaderLast;
}

bool VertexShaderCache::CompileVertexShader(VERTEXSHADER& vs, const char* pstrprogram)
{
	// Reset GL error before compiling shaders. Yeah, we need to investigate the causes of these.
	GLenum err = GL_REPORT_ERROR();
	if (err != GL_NO_ERROR)
	{
		ERROR_LOG(VIDEO, "glError %08x before VS!", err);
	}

#if defined HAVE_CG && HAVE_CG
	char stropt[64];
	sprintf(stropt, "MaxLocalParams=256,MaxInstructions=%d", s_nMaxVertexInstructions);
	const char *opts[] = {"-profileopts", stropt, "-O2", "-q", NULL};
	CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgvProf, "main", opts);
	if (!cgIsProgram(tempprog)) {
        if (s_displayCompileAlert) {
            PanicAlert("Failed to create vertex shader");
            s_displayCompileAlert = false;
        }
        cgDestroyProgram(tempprog);
		ERROR_LOG(VIDEO, "Failed to load vs %s:", cgGetLastListing(g_cgcontext));
		ERROR_LOG(VIDEO, pstrprogram);
		return false;
	}

	if (cgGetError() != CG_NO_ERROR)
	{
		WARN_LOG(VIDEO, "Failed to load vs %s:", cgGetLastListing(g_cgcontext));
		WARN_LOG(VIDEO, pstrprogram);
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
	glGenProgramsARB(1, &vs.glprogid);
	SetCurrentShader(vs.glprogid);

	glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pcompiledprog), pcompiledprog);	
	err = GL_REPORT_ERROR();
	if (err != GL_NO_ERROR) {
		ERROR_LOG(VIDEO, pstrprogram);
		ERROR_LOG(VIDEO, pcompiledprog);
	}

	cgDestroyProgram(tempprog);
#endif

#if defined(_DEBUG) || defined(DEBUGFAST) 
	vs.strprog = pstrprogram;
#endif

	return true;
}

void VertexShaderCache::DisableShader()
{
	if (ShaderEnabled)
	{
		glDisable(GL_VERTEX_PROGRAM_ARB);
		ShaderEnabled = false;
	}
}


void VertexShaderCache::SetCurrentShader(GLuint Shader)
{
	if (!ShaderEnabled)
	{
		glEnable(GL_VERTEX_PROGRAM_ARB);
		ShaderEnabled= true;		
	}
	if (CurrentShader != Shader)
	{
		if(Shader != 0)
			CurrentShader = Shader;
		glBindProgramARB(GL_VERTEX_PROGRAM_ARB, CurrentShader);
	}
}

}
