// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <math.h>

#include "Globals.h"
#include "VideoConfig.h"
#include "Statistics.h"

#include "GLUtil.h"

#include "Render.h"
#include "VertexShaderGen.h"
#include "VertexShaderManager.h"
#include "ProgramShaderCache.h"
#include "VertexManager.h"
#include "VertexLoader.h"
#include "XFMemory.h"
#include "ImageWrite.h"
#include "FileUtil.h"
#include "Debugger.h"

namespace OGL
{

void SetVSConstant4fvByName(const char * name, unsigned int offset, const float *f, const unsigned int count = 1)
{
	ProgramShaderCache::PCacheEntry tmp = ProgramShaderCache::GetShaderProgram();
	for (int a = 0; a < NUM_UNIFORMS; ++a)
	{
		if (!strcmp(name, UniformNames[a]))
		{
			if (tmp.shader.UniformLocations[a] == -1)
				return;
			else
			{
				glUniform4fv(tmp.shader.UniformLocations[a] + offset, count, f);
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
