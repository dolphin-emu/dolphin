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

#include <fstream>
#include <vector>

// Common
#include "MemoryUtil.h"
#include "FileUtil.h"

// VideoCommon
#include "Fifo.h"
#include "VideoConfig.h"
#include "Statistics.h"
#include "Profiler.h"
#include "ImageWrite.h"
#include "BPMemory.h"
#include "IndexGenerator.h"
#include "OpcodeDecoding.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "VertexShaderGen.h"
#include "VertexLoader.h"

// OGL
#include "OGL_Render.h"
#include "OGL_TextureCache.h"
#include "OGL_PixelShaderCache.h"
#include "OGL_VertexShaderCache.h"
#include "OGL_VertexManager.h"

#include "../Main.h"

namespace OGL
{

enum
{
	MAXVBUFFERSIZE =	0x1FFFF,
	MAXIBUFFERSIZE =	0xFFFF,
	MAXVBOBUFFERCOUNT =	0x8,
};

//static GLuint s_vboBuffers[MAXVBOBUFFERCOUNT] = {0};
//static int s_nCurVBOIndex = 0; // current free buffer

VertexManager::VertexManager()
{
	lastPrimitive = GX_DRAW_NONE;

	//s_nCurVBOIndex = 0;
	//glGenBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	glEnableClientState(GL_VERTEX_ARRAY);
	g_nativeVertexFmt = NULL;

	GL_REPORT_ERRORD();
}

VertexManager::~VertexManager()
{
	//glDeleteBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	//s_nCurVBOIndex = 0;
}

void VertexManager::Draw(u32 /*stride*/, bool alphapass)
{
	if (alphapass)
	{
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
		glDisable(GL_BLEND);
	}

	if (IndexGenerator::GetNumTriangles() > 0)
	{
		glDrawElements(GL_TRIANGLES, IndexGenerator::GetTriangleindexLen(), GL_UNSIGNED_SHORT, TIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumLines() > 0)
	{
		glDrawElements(GL_LINES, IndexGenerator::GetLineindexLen(), GL_UNSIGNED_SHORT, LIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumPoints() > 0)
	{
		glDrawElements(GL_POINTS, IndexGenerator::GetPointindexLen(), GL_UNSIGNED_SHORT, PIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}

	if (alphapass)
	{	
		// restore color mask
		g_renderer->SetColorMask();

		if (bpmem.blendmode.blendenable || bpmem.blendmode.subtract) 
			glEnable(GL_BLEND);
	}
}

}
