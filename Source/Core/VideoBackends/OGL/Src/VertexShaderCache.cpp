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

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
	{
		ProgramShaderCache::SetMultiVSConstant4fv(const_number, f, count);
		return;
	}
	ProgramShaderCache::PCacheEntry tmp = ProgramShaderCache::GetShaderProgram();
	for (unsigned int a = 0; a < 9; ++a)
	{
		u32 offset = VSVar_Loc[a].reg - const_number;
		if(offset >= count) return;
		u32 size = std::min(tmp.shader.UniformSize[a+10], count-offset);
		if(size > 0)
			glUniform4fv(tmp.shader.UniformLocations[a+10], size, f + 4*offset);
	}
}

}  // namespace OGL
