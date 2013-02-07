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

void VertexShaderCache::Init()
{
	ShaderEnabled = true;
	CurrentShader = 0;
	last_entry = NULL;
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
	GLuint result = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(result, 1, &pstrprogram, NULL);
	glCompileShader(result);
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(DEBUG_GLSL)
	GLsizei length = 0;
	glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);
	if (length > 1)
	{
		GLsizei charsWritten;
		GLchar* infoLog = new GLchar[length];
		glGetShaderInfoLog(result, length, &charsWritten, infoLog);
		ERROR_LOG(VIDEO, "VS Shader info log:\n%s", infoLog);
		char szTemp[MAX_PATH];
		sprintf(szTemp, "vs_%d.txt", result);
		FILE *fp = fopen(szTemp, "wb");
		fwrite(infoLog, strlen(infoLog), 1, fp);
		fwrite(pstrprogram, strlen(pstrprogram), 1, fp);
		fclose(fp);
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
#endif
	(void)GL_REPORT_ERROR();
	vs.glprogid = result;
	return true;
}

void SetVSConstant4fvByName(const char * name, unsigned int offset, const float *f, const unsigned int count = 1)
{
	ProgramShaderCache::PCacheEntry tmp = ProgramShaderCache::GetShaderProgram();
	for (int a = 0; a < NUM_UNIFORMS; ++a)
	{
		if (!strcmp(name, UniformNames[a]))
		{
			if (tmp.UniformLocations[a] == -1)
				return;
			else
			{
				glUniform4fv(tmp.UniformLocations[a] + offset, count, f);
				return;
			}
		}
	}
}

void Renderer::SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	float const buf[4] = {f1, f2, f3, f4};

	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetMultiVSConstant4fv(const_number, buf, 1);
		return;
	}
	for (unsigned int a = 0; a < 9; ++a)
	{
		if (const_number >= VSVar_Loc[a].reg && const_number < ( VSVar_Loc[a].reg + VSVar_Loc[a].size))
		{
			unsigned int offset = const_number - VSVar_Loc[a].reg;
			SetVSConstant4fvByName(VSVar_Loc[a].name, offset, buf);
			return;
		}
	}
}

void Renderer::SetVSConstant4fv(unsigned int const_number, const float *f)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetMultiVSConstant4fv(const_number, f, 1);
		return;
	}
	for (unsigned int a = 0; a < 9; ++a)
	{
		if (const_number >= VSVar_Loc[a].reg && const_number < ( VSVar_Loc[a].reg + VSVar_Loc[a].size))
		{
			unsigned int offset = const_number - VSVar_Loc[a].reg;
			SetVSConstant4fvByName(VSVar_Loc[a].name, offset, f);
			return;
		}
	}
}

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetMultiVSConstant4fv(const_number, f, count);
		return;
	}
	for (unsigned int a = 0; a < 9; ++a)
	{
		if (const_number >= VSVar_Loc[a].reg && const_number < ( VSVar_Loc[a].reg + VSVar_Loc[a].size))
		{
			unsigned int offset = const_number - VSVar_Loc[a].reg;
			SetVSConstant4fvByName(VSVar_Loc[a].name, offset, f, count);
			return;
		}
	}
}

void Renderer::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f)
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
		ProgramShaderCache::SetMultiVSConstant4fv(const_number, buf, count);
		return;
	}
	for (unsigned int a = 0; a < 9; ++a)
	{
		if (const_number >= VSVar_Loc[a].reg && const_number < ( VSVar_Loc[a].reg + VSVar_Loc[a].size))
		{
			unsigned int offset = const_number - VSVar_Loc[a].reg;
			SetVSConstant4fvByName(VSVar_Loc[a].name, offset, buf, count);
			return;
		}
	}
}

}  // namespace OGL
