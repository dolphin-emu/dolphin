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

#include "Globals.h"
#include "VideoConfig.h"
#include "Statistics.h"

#include "GLUtil.h"

#include "Render.h"
#include "VertexShaderGen.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexManager.h"
#include "VertexLoader.h"
#include "XFMemory.h"
#include "ImageWrite.h"
#include "FileUtil.h"
#include "Debugger.h"

namespace OGL
{

VertexShaderCache::VSCache VertexShaderCache::vshaders;
GLuint VertexShaderCache::CurrentShader;
bool VertexShaderCache::ShaderEnabled;

VertexShaderCache::VSCacheEntry* VertexShaderCache::last_entry = NULL;
VERTEXSHADERUID VertexShaderCache::last_uid;

static int s_nMaxVertexInstructions;


void VertexShaderCache::Init()
{
	glEnable(GL_VERTEX_PROGRAM_ARB);
	ShaderEnabled = true;
	CurrentShader = 0;
	last_entry = NULL;

	glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, (GLint *)&s_nMaxVertexInstructions);		
	if (strstr((const char*)glGetString(GL_VENDOR), "Humper") != NULL) s_nMaxVertexInstructions = 4096;
#if CG_VERSION_NUM == 2100
	if (strstr((const char*)glGetString(GL_VENDOR), "ATI") != NULL)
	{
		s_nMaxVertexInstructions = 4096;
	}
#endif
}

void VertexShaderCache::Shutdown()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); ++iter)
		iter->second.Destroy();
	vshaders.clear();
}


VERTEXSHADER* VertexShaderCache::SetShader(u32 components)
{
	VERTEXSHADERUID uid;
	GetVertexShaderId(&uid, components);
	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
			ValidateVertexShaderIDs(API_OPENGL, vshaders[uid].safe_uid, vshaders[uid].shader.strprog, components);
			return &last_entry->shader;
		}
	}

	last_uid = uid;

	VSCache::iterator iter = vshaders.find(uid);
	if (iter != vshaders.end())
	{
		VSCacheEntry &entry = iter->second;
		last_entry = &entry;

		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
		ValidateVertexShaderIDs(API_OPENGL, entry.safe_uid, entry.shader.strprog, components);
		return &last_entry->shader;
	}

	// Make an entry in the table
	VSCacheEntry& entry = vshaders[uid];
	last_entry = &entry;
	const char *code = GenerateVertexShaderCode(components, API_OPENGL);
	GetSafeVertexShaderId(&entry.safe_uid, components);

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS && code) {
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%svs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);

		SaveData(szTemp, code);
	}
#endif

	if (!code || !VertexShaderCache::CompileVertexShader(entry.shader, code)) {
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return NULL;
	}

	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, vshaders.size());
	GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	return &last_entry->shader;
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
		static int num_failures = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%sbad_vs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
		std::ofstream file(szTemp);
		file << pstrprogram;
		file.close();

		PanicAlert("Failed to compile vertex shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%d):\n%s",
						szTemp,
						g_cgfProf,
						cgGetLastListing(g_cgcontext));

        cgDestroyProgram(tempprog);
		ERROR_LOG(VIDEO, "Failed to load vs %s:", cgGetLastListing(g_cgcontext));
		ERROR_LOG(VIDEO, "%s", pstrprogram);
		return false;
	}

	if (cgGetError() != CG_NO_ERROR)
	{
		WARN_LOG(VIDEO, "Failed to load vs %s:", cgGetLastListing(g_cgcontext));
		WARN_LOG(VIDEO, "%s", pstrprogram);
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
		ERROR_LOG(VIDEO, "%s", pstrprogram);
		ERROR_LOG(VIDEO, "%s", pcompiledprog);
	}

	cgDestroyProgram(tempprog);
#endif

	if (g_ActiveConfig.bEnableShaderDebugging)
		vs.strprog = pstrprogram;

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

void Renderer::SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, const_number, f1, f2, f3, f4);
}

void Renderer::SetVSConstant4fv(unsigned int const_number, const float *f)
{
	glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number, f);
}

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
    if(GLEW_EXT_gpu_program_parameters)
    {
       glProgramEnvParameters4fvEXT(GL_VERTEX_PROGRAM_ARB, const_number, count, f);
    }
    else
    {
        for (unsigned int i = 0; i < count; i++,f+=4)
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number + i, f);
    }
}

void Renderer::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
{
   if(GLEW_EXT_gpu_program_parameters)
	{
        float buf[4 * C_VENVCONST_END];
        for (unsigned int i = 0; i < count; i++)
	    {
		    buf[4*i  ] = *f++;
    		buf[4*i+1] = *f++;
	    	buf[4*i+2] = *f++;
	    	buf[4*i+3] = 0.f;
	    }
        glProgramEnvParameters4fvEXT(GL_VERTEX_PROGRAM_ARB, const_number, count, buf);
    }
    else
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
}

}  // namespace OGL
