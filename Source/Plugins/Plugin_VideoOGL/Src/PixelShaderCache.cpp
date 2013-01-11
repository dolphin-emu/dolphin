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

#include "Globals.h"

#include "GLUtil.h"

#include <cmath>

#include "Statistics.h"
#include "VideoConfig.h"
#include "ImageWrite.h"
#include "Common.h"
#include "Render.h"
#include "VertexShaderGen.h"
#include "ProgramShaderCache.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "OnScreenDisplay.h"
#include "StringUtil.h"
#include "FileUtil.h"
#include "Debugger.h"

namespace OGL
{

static int s_nMaxPixelInstructions;
PixelShaderCache::PSCache PixelShaderCache::PixelShaders;
PIXELSHADERUID PixelShaderCache::s_curuid;
bool PixelShaderCache::s_displayCompileAlert;
GLuint PixelShaderCache::CurrentShader;
bool PixelShaderCache::ShaderEnabled;

PixelShaderCache::PSCacheEntry* PixelShaderCache::last_entry = NULL;
PIXELSHADERUID PixelShaderCache::last_uid;

void PixelShaderCache::Init()
{
	ShaderEnabled = true;
	CurrentShader = 0;
	last_entry = NULL;
	GL_REPORT_ERRORD();

	s_displayCompileAlert = true;
}

void PixelShaderCache::Shutdown()
{
	PSCache::iterator iter = PixelShaders.begin();
	for (; iter != PixelShaders.end(); iter++)
		iter->second.Destroy();
	PixelShaders.clear();
}

FRAGMENTSHADER* PixelShaderCache::SetShader(DSTALPHA_MODE dstAlphaMode, u32 components)
{
	PIXELSHADERUID uid;
	GetPixelShaderId(&uid, dstAlphaMode, components);

	// Check if the shader is already set
	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
			ValidatePixelShaderIDs(API_OPENGL, last_entry->safe_uid, last_entry->shader.strprog, dstAlphaMode, components);
			return &last_entry->shader;
		}
	}

	last_uid = uid;

	PSCache::iterator iter = PixelShaders.find(uid);
	if (iter != PixelShaders.end())
	{
		PSCacheEntry &entry = iter->second;
		last_entry = &entry;

		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
		ValidatePixelShaderIDs(API_OPENGL, entry.safe_uid, entry.shader.strprog, dstAlphaMode, components);
		return &last_entry->shader;
	}

	// Make an entry in the table
	PSCacheEntry& newentry = PixelShaders[uid];
	last_entry = &newentry;
	const char *code = GeneratePixelShaderCode(dstAlphaMode, API_OPENGL, components);

	if (g_ActiveConfig.bEnableShaderDebugging && code)
	{
		GetSafePixelShaderId(&newentry.safe_uid, dstAlphaMode, components);
		newentry.shader.strprog = code;
	}

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS && code) {
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%sps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);

		SaveData(szTemp, code);
	}
#endif

	if (!code || !CompilePixelShader(newentry.shader, code)) {
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return NULL;
	}

	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, PixelShaders.size());
	GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	return &last_entry->shader;
}

bool PixelShaderCache::CompilePixelShader(FRAGMENTSHADER& ps, const char* pstrprogram)
{
	GLuint result = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(result, 1, &pstrprogram, NULL);
	glCompileShader(result);

	GLint compileStatus;
	glGetShaderiv(result, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE)
	{
		// Compile failed
		ERROR_LOG(VIDEO, "Shader compilation failed; see info log");

		GLsizei length = 0;
		glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);
		if (length > 0)
		{
			GLsizei charsWritten;
			GLchar* infoLog = new GLchar[length];
			glGetShaderInfoLog(result, length, &charsWritten, infoLog);
			WARN_LOG(VIDEO, "PS Shader info log:\n%s", infoLog);
			char szTemp[MAX_PATH];
			sprintf(szTemp, "ps_%d.txt", result);
			FILE *fp = fopen(szTemp, "wb");
			fwrite(pstrprogram, strlen(pstrprogram), 1, fp);
			fclose(fp);
			delete[] infoLog;
		}
		// Don't try to use this shader
		glDeleteShader(result);
		return false;
	}

	(void)GL_REPORT_ERROR();
	ps.glprogid = result;
	return true;
}

void SetPSConstant4fvByName(const char * name, unsigned int offset, const float *f, const unsigned int count = 1)
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

// Renderer functions
void Renderer::SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	float const f[4] = {f1, f2, f3, f4};

	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetMultiPSConstant4fv(const_number, f, 1);
		return;
	}
	for (unsigned int a = 0; a < 10; ++a)
	{
		if (const_number >= PSVar_Loc[a].reg && const_number < (PSVar_Loc[a].reg + PSVar_Loc[a].size))
		{
			unsigned int offset = const_number - PSVar_Loc[a].reg;
			SetPSConstant4fvByName(PSVar_Loc[a].name, offset, f);
			return;
		}
	}
}

void Renderer::SetPSConstant4fv(unsigned int const_number, const float *f)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetMultiPSConstant4fv(const_number, f, 1);
		return;
	}
	for (unsigned int a = 0; a < 10; ++a)
	{
		if (const_number >= PSVar_Loc[a].reg && const_number < (PSVar_Loc[a].reg + PSVar_Loc[a].size))
		{
			unsigned int offset = const_number - PSVar_Loc[a].reg;
			SetPSConstant4fvByName(PSVar_Loc[a].name, offset, f);
			return;
		}
	}
}

void Renderer::SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetMultiPSConstant4fv(const_number, f, count);
		return;
	}
	for (unsigned int a = 0; a < 10; ++a)
	{
		if (const_number >= PSVar_Loc[a].reg && const_number < (PSVar_Loc[a].reg + PSVar_Loc[a].size))
		{
			unsigned int offset = const_number - PSVar_Loc[a].reg;
			SetPSConstant4fvByName(PSVar_Loc[a].name, offset, f, count);
			return;
		}
	}
}
}  // namespace OGL
