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
#include <assert.h>

#include "Globals.h"
#include "VideoConfig.h"
#include "Statistics.h"

#include "GLUtil.h"

#include "Render.h"
#include "VertexShaderGen.h"
#include "VertexShaderManager.h"
#include "ProgramShaderCache.h"
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
void (*pSetVSConstant4f)(unsigned int, float, float, float, float);
void (*pSetVSConstant4fv)(unsigned int, const float*);
void (*pSetMultiVSConstant4fv)(unsigned int, unsigned int, const float*);
void (*pSetMultiVSConstant3fv)(unsigned int, unsigned int, const float*);
bool (*pCompileVertexShader)(VERTEXSHADER&, const char*);

void VertexShaderCache::Init()
{
    ShaderEnabled = true;
    CurrentShader = 0;
    last_entry = NULL;

    if(g_ActiveConfig.bUseGLSL)
    {
        pSetVSConstant4f = SetGLSLVSConstant4f;
        pSetVSConstant4fv = SetGLSLVSConstant4fv;
        pSetMultiVSConstant4fv = SetMultiGLSLVSConstant4fv;
        pSetMultiVSConstant3fv = SetMultiGLSLVSConstant3fv;
        pCompileVertexShader = CompileGLSLVertexShader;
    }
    else
    {
        pSetVSConstant4f = SetCGVSConstant4f;
        pSetVSConstant4fv = SetCGVSConstant4fv;
        pSetMultiVSConstant4fv = SetMultiCGVSConstant4fv;
        pSetMultiVSConstant3fv = SetMultiCGVSConstant3fv;
        pCompileVertexShader = CompileCGVertexShader;
        glEnable(GL_VERTEX_PROGRAM_ARB);
    }

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
    const char *code = GenerateVertexShaderCode(components, g_ActiveConfig.bUseGLSL ? API_GLSL : API_OPENGL);
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
	return pCompileVertexShader(vs, pstrprogram);
}

void VertexShaderCache::DisableShader()
{
	if(g_ActiveConfig.bUseGLSL)
		assert(true);
    if (ShaderEnabled)
    {
        glDisable(GL_VERTEX_PROGRAM_ARB);
        ShaderEnabled = false;
    }
}


void VertexShaderCache::SetCurrentShader(GLuint Shader)
{
	if(g_ActiveConfig.bUseGLSL)
		assert(true);
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
// GLSL Specific
bool CompileGLSLVertexShader(VERTEXSHADER& vs, const char* pstrprogram)
{
	GLuint result = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(result, 1, &pstrprogram, NULL);
	glCompileShader(result);
	GLsizei length = 0;

	glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);
	if (length > 0)
	{
		GLsizei charsWritten;
		GLchar* infoLog = new GLchar[length];
		glGetShaderInfoLog(result, length, &charsWritten, infoLog);
		WARN_LOG(VIDEO, "VS Shader info log:\n%s", infoLog);
				char szTemp[MAX_PATH];
				sprintf(szTemp, "vs_%d.txt", result);
				FILE *fp = fopen(szTemp, "wb");
				fwrite(pstrprogram, strlen(pstrprogram), 1, fp);
				fclose(fp);
		
		if(strstr(infoLog, "warning") != NULL || strstr(infoLog, "error") != NULL)
			exit(0);
		delete[] infoLog;
	}

	GLint compileStatus;
	glGetShaderiv(result, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE)
	{
		// Compile failed
		ERROR_LOG(VIDEO, "Shader compilation failed; see info log");
		// Don't try to use this shader
		glDeleteShader(result);
		return false;
	}

	(void)GL_REPORT_ERROR();
	vs.glprogid = result;
	vs.bGLSL = true;
	return true;
}
void SetVSConstant4fvByName(const char * name, unsigned int offset, const float *f, const unsigned int count = 1)
{
    PROGRAMSHADER tmp = ProgramShaderCache::GetShaderProgram();
    for(int a = 0; a < NUM_UNIFORMS; ++a)
        if(!strcmp(name, UniformNames[a]))
        {
            if(tmp.UniformLocations[a] == -1)
                return;
            else
            {
                glUniform4fv(tmp.UniformLocations[a] + offset, count, f);
                return;
            }
        }
}
void SetGLSLVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
    float buf[4];
    buf[0] = f1;
    buf[1] = f2;
    buf[2] = f3;
    buf[3] = f4;
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetUniformObjects(1, const_number, buf);
		//return;
	}
    for( unsigned int a = 0; a < 9; ++a)
    {
        if( const_number >= VSVar_Loc[a].reg && const_number < ( VSVar_Loc[a].reg + VSVar_Loc[a].size))
        {
            unsigned int offset = const_number - VSVar_Loc[a].reg;
            SetVSConstant4fvByName(VSVar_Loc[a].name, offset, buf);
            return;
        }
    }
}

void SetGLSLVSConstant4fv(unsigned int const_number, const float *f)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetUniformObjects(1, const_number, f);
		//return;
	}
    for( unsigned int a = 0; a < 9; ++a)
    {
        if( const_number >= VSVar_Loc[a].reg && const_number < ( VSVar_Loc[a].reg + VSVar_Loc[a].size))
        {
            unsigned int offset = const_number - VSVar_Loc[a].reg;
            SetVSConstant4fvByName(VSVar_Loc[a].name, offset, f);
            return;
        }
    }
}

void SetMultiGLSLVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetUniformObjects(1, const_number, f, count);
		//return;
	}
    for( unsigned int a = 0; a < 9; ++a)
    {
        if( const_number >= VSVar_Loc[a].reg && const_number < ( VSVar_Loc[a].reg + VSVar_Loc[a].size))
        {
            unsigned int offset = const_number - VSVar_Loc[a].reg;
            SetVSConstant4fvByName(VSVar_Loc[a].name, offset, f, count);
            return;
        }
    }
}

void SetMultiGLSLVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
{
    float buf[4 * C_VENVCONST_END];
    for (unsigned int i = 0; i < count; i++)
    {
        buf[4*i  ] = *f++;
        buf[4*i+1] = *f++;
        buf[4*i+2] = *f++;
        buf[4*i+3] = 0.f;
    }
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetUniformObjects(1, const_number, buf, count);
		//return;
	}
    for( unsigned int a = 0; a < 9; ++a)
    {
        if( const_number >= VSVar_Loc[a].reg && const_number < ( VSVar_Loc[a].reg + VSVar_Loc[a].size))
        {
            unsigned int offset = const_number - VSVar_Loc[a].reg;
            SetVSConstant4fvByName(VSVar_Loc[a].name, offset, buf, count);
            return;
        }
    }
}

// CG Specific
bool CompileCGVertexShader(VERTEXSHADER& vs, const char* pstrprogram)
{
    // Reset GL error before compiling shaders. Yeah, we need to investigate the causes of these.
    GLenum err = GL_REPORT_ERROR();
    if (err != GL_NO_ERROR)
    {
        ERROR_LOG(VIDEO, "glError %08x before VS!", err);
    }

#if defined HAVE_CG && HAVE_CG
    CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgvProf, "main", NULL);
    if (!cgIsProgram(tempprog)) {
        static int num_failures = 0;
        char szTemp[MAX_PATH];
        sprintf(szTemp, "bad_vs_%04i.txt", num_failures++);
        std::ofstream file(szTemp);
        file << pstrprogram;
        file.close();

        PanicAlert("Failed to compile vertex shader %d!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%d):\n%s",
                   num_failures - 1, szTemp,
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
    vs.bGLSL = false;
    VertexShaderCache::SetCurrentShader(vs.glprogid);

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
void SetCGVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
    glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, const_number, f1, f2, f3, f4);
}

void SetCGVSConstant4fv(unsigned int const_number, const float *f)
{
    glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number, f);
}

void SetMultiCGVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
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

void SetMultiCGVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
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

// Renderer Functions
void Renderer::SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
        pSetVSConstant4f(const_number, f1, f2, f3, f4);
}

void Renderer::SetVSConstant4fv(unsigned int const_number, const float *f)
{
        pSetVSConstant4fv(const_number, f);
}

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
        pSetMultiVSConstant4fv(const_number, count, f);
}

void Renderer::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
{
        pSetMultiVSConstant3fv(const_number, count, f);
}


}  // namespace OGL
