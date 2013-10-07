// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
#include "PixelShaderManager.h"
#include "OnScreenDisplay.h"
#include "StringUtil.h"
#include "FileUtil.h"
#include "Debugger.h"

namespace OGL
{

// Renderer functions
void Renderer::SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetMultiPSConstant4fv(const_number, f, count);
		return;
	}
	
	ProgramShaderCache::PCacheEntry tmp = ProgramShaderCache::GetShaderProgram();
	for (unsigned int a = 0; a < 10; ++a)
	{
		u32 offset = PSVar_Loc[a].reg - const_number;
		if(offset >= count) return;
		u32 size = std::min(tmp.shader.UniformSize[a], count-offset);
		if(size > 0)
			glUniform4fv(tmp.shader.UniformLocations[a], size, f + 4*offset);
	}
}
}  // namespace OGL
