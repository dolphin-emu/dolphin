// Copyright (C) 2003-2009 Dolphin Project.

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

#include <fstream>
#include <vector>

#include "Config.h"
#include "Statistics.h"
#include "MemoryUtil.h"
#include "Profiler.h"
#include "Render.h"
#include "ImageWrite.h"
#include "BPMemory.h"
#include "TextureMngr.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexShaderManager.h"
#include "VertexShaderGen.h"
#include "VertexLoader.h"
#include "VertexManager.h"

#define MAX_BUFFER_SIZE 0x4000

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace VertexManager
{

static GLuint s_vboBuffers[0x40] = {0};
static int s_nCurVBOIndex = 0; // current free buffer
static u8 *s_pBaseBufferPointer = NULL;
static std::vector< GLint > s_vertexFirstOffset;
static std::vector< GLsizei > s_vertexGroupSize;
static std::vector< std::pair< GLenum, int > > s_vertexGroups;
u32 s_vertexCount;

static const GLenum c_primitiveType[8] =
{
    GL_QUADS,
    GL_ZERO, //nothing
    GL_TRIANGLES,
    GL_TRIANGLE_STRIP,
    GL_TRIANGLE_FAN,
    GL_LINES,
    GL_LINE_STRIP,
    GL_POINTS
};

bool Init()
{
	s_pBaseBufferPointer = (u8*)AllocateMemoryPages(MAX_BUFFER_SIZE);
	s_pCurBufferPointer = s_pBaseBufferPointer;

	s_nCurVBOIndex = 0;
	glGenBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	for (u32 i = 0; i < ARRAYSIZE(s_vboBuffers); ++i) {
		glBindBuffer(GL_ARRAY_BUFFER, s_vboBuffers[i]);
		glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, NULL, GL_STREAM_DRAW);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	g_nativeVertexFmt = NULL;
	GL_REPORT_ERRORD();

	return true;
}

void Shutdown()
{
	FreeMemoryPages(s_pBaseBufferPointer, MAX_BUFFER_SIZE); s_pBaseBufferPointer = s_pCurBufferPointer = NULL;
	glDeleteBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	memset(s_vboBuffers, 0, sizeof(s_vboBuffers));

	s_vertexFirstOffset.resize(0);
	s_vertexGroupSize.resize(0);
	s_vertexGroups.resize(0);
	s_vertexCount = 0;
	s_nCurVBOIndex = 0;
	ResetBuffer();
}

void ResetBuffer()
{
	s_nCurVBOIndex = (s_nCurVBOIndex + 1) % ARRAYSIZE(s_vboBuffers);
	s_pCurBufferPointer = s_pBaseBufferPointer;
	s_vertexFirstOffset.resize(0);
	s_vertexGroupSize.resize(0);
	s_vertexGroups.resize(0);
	s_vertexCount = 0;
}

int GetRemainingSize()
{
	return MAX_BUFFER_SIZE - (int)(s_pCurBufferPointer - s_pBaseBufferPointer);
}

void AddVertices(int primitive, int numvertices)
{
	_assert_(numvertices > 0);
	_assert_(g_nativeVertexFmt != NULL);

	ADDSTAT(stats.thisFrame.numPrims, numvertices);
	if (!s_vertexGroups.empty() && s_vertexGroups.back().first == c_primitiveType[primitive]) {
		// We can join primitives for free here. Not likely to help much, though, but whatever...
		if (c_primitiveType[primitive] == GL_TRIANGLES ||
			c_primitiveType[primitive] == GL_LINES ||
			c_primitiveType[primitive] == GL_POINTS ||
			c_primitiveType[primitive] == GL_QUADS) {
			INCSTAT(stats.thisFrame.numPrimitiveJoins);
			// Easy join
			s_vertexGroupSize.back() += numvertices;
			s_vertexCount += numvertices;
			return;
		}
	}

	s_vertexFirstOffset.push_back(s_vertexCount);
	s_vertexGroupSize.push_back(numvertices);
	s_vertexCount += numvertices;
	if (!s_vertexGroups.empty() && s_vertexGroups.back().first == c_primitiveType[primitive])
		s_vertexGroups.back().second++;
	else
		s_vertexGroups.push_back(std::make_pair(c_primitiveType[primitive], 1));

#if defined(_DEBUG) || defined(DEBUGFAST) 
	static const char *sprims[8] = {"quads", "nothing", "tris", "tstrip", "tfan", "lines", "lstrip", "points"};
	PRIM_LOG("prim: %s, c=%d", sprims[primitive], numvertices);
#endif
}

void Flush()
{
	if (s_vertexCount == 0)
		return;

	_assert_(s_pCurBufferPointer != s_pBaseBufferPointer);

#if defined(_DEBUG) || defined(DEBUGFAST) 
	PRIM_LOG("frame%d:\n texgen=%d, numchan=%d, dualtex=%d, ztex=%d, cole=%d, alpe=%d, ze=%d", g_Config.iSaveTargetId, xfregs.numTexGens,
		xfregs.nNumChans, (int)xfregs.bEnableDualTexTransform, bpmem.ztex2.op,
		bpmem.blendmode.colorupdate, bpmem.blendmode.alphaupdate, bpmem.zmode.updateenable);

	for (int i = 0; i < xfregs.nNumChans; ++i) 
	{
		LitChannel* ch = &xfregs.colChans[i].color;
		PRIM_LOG("colchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
		ch = &xfregs.colChans[i].alpha;
		PRIM_LOG("alpchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
	}

	for (int i = 0; i < xfregs.numTexGens; ++i) 
	{
		TexMtxInfo tinfo = xfregs.texcoords[i].texmtxinfo;
		if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP) tinfo.hex &= 0x7ff;
		if (tinfo.texgentype != XF_TEXGEN_REGULAR) tinfo.projection = 0;

		PRIM_LOG("txgen%d: proj=%d, input=%d, gentype=%d, srcrow=%d, embsrc=%d, emblght=%d, postmtx=%d, postnorm=%d",
			i, tinfo.projection, tinfo.inputform, tinfo.texgentype, tinfo.sourcerow, tinfo.embosssourceshift, tinfo.embosslightshift,
			xfregs.texcoords[i].postmtxinfo.index, xfregs.texcoords[i].postmtxinfo.normalize);
	}

	PRIM_LOG("pixel: tev=%d, ind=%d, texgen=%d, dstalpha=%d, alphafunc=0x%x", bpmem.genMode.numtevstages+1, bpmem.genMode.numindstages,
		bpmem.genMode.numtexgens, (u32)bpmem.dstalpha.enable, (bpmem.alphaFunc.hex>>16)&0xff);
#endif

	DVSTARTPROFILE();

	GL_REPORT_ERRORD(); 

	glBindBuffer(GL_ARRAY_BUFFER, s_vboBuffers[s_nCurVBOIndex]);
	glBufferData(GL_ARRAY_BUFFER, s_pCurBufferPointer - s_pBaseBufferPointer, s_pBaseBufferPointer, GL_STREAM_DRAW);
	GL_REPORT_ERRORD();

	// setup the pointers
	if (g_nativeVertexFmt)
		g_nativeVertexFmt->SetupVertexPointers();
	GL_REPORT_ERRORD();

	// set the textures
	DVSTARTSUBPROFILE("VertexManager::Flush:textures");

	u32 usedtextures = 0;
	for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
		if (bpmem.tevorders[i / 2].getEnable(i & 1))
			usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);

	if (bpmem.genMode.numindstages > 0)
		for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages) 
				usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);

	for (int i = 0; i < 8; i++) 
	{
		if (usedtextures & (1 << i)) 
		{
			glActiveTexture(GL_TEXTURE0 + i);

			FourTexUnits &tex = bpmem.tex[i >> 2];
			TextureMngr::TCacheEntry* tentry = TextureMngr::Load(i, (tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, tex.texTlut[i&3].tlut_format);

			if (tentry != NULL) 
			{
				// 0s are probably for no manual wrapping needed.
				PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, 0, 0);

				// texture is hires - pass the scaling size
				if (tentry->scaleX != 1.0f || tentry->scaleY != 1.0f)
					PixelShaderManager::SetCustomTexScale(i, tentry->scaleX, tentry->scaleY);
				if (g_Config.iLog & CONF_SAVETEXTURES) 
				{
					// save the textures
					char strfile[255];
					sprintf(strfile, "%sframes/tex%.3d_%d.tga", FULL_DUMP_DIR, g_Config.iSaveTargetId, i);
					SaveTexture(strfile, GL_TEXTURE_2D, tentry->texture, tentry->w, tentry->h);
				}
			}
			else
				ERROR_LOG(VIDEO, "error loading tex\n");
		}
	}

	FRAGMENTSHADER* ps = PixelShaderCache::GetShader(false);
	VERTEXSHADER* vs = VertexShaderCache::GetShader(g_nativeVertexFmt->m_components);

	// set global constants
    VertexShaderManager::SetConstants(g_Config.bProjHack1,g_Config.bPhackvalue1, g_Config.fhackvalue1, g_Config.bPhackvalue2, g_Config.fhackvalue2, g_Config.bFreeLook);
	PixelShaderManager::SetConstants();

	// finally bind

	// TODO - cache progid, check if same as before. Maybe GL does this internally, though.
	// This is the really annoying problem with GL - you never know whether it's worth caching stuff yourself.
	if (vs) glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vs->glprogid);
	if (ps) glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ps->glprogid); // Lego Star Wars crashes here.

#if defined(_DEBUG) || defined(DEBUGFAST) 
	PRIM_LOG("");
#endif

	int groupStart = 0;
	for (unsigned i = 0; i < s_vertexGroups.size(); i++)
	{
		INCSTAT(stats.thisFrame.numDrawCalls);
		glMultiDrawArrays(s_vertexGroups[i].first,
		                  &s_vertexFirstOffset[groupStart],
		                  &s_vertexGroupSize[groupStart], 
		                  s_vertexGroups[i].second);
		groupStart += s_vertexGroups[i].second;
	}

    // run through vertex groups again to set alpha
    if (!g_Config.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate) 
	{
        ps = PixelShaderCache::GetShader(true);

        if (ps) glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ps->glprogid);

        // only update alpha
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

        glDisable(GL_BLEND);

        groupStart = 0;
	    for (unsigned i = 0; i < s_vertexGroups.size(); i++)
	    {
		    INCSTAT(stats.thisFrame.numDrawCalls);
		    glMultiDrawArrays(s_vertexGroups[i].first,
		                      &s_vertexFirstOffset[groupStart],
		                      &s_vertexGroupSize[groupStart], 
		                      s_vertexGroups[i].second);
		    groupStart += s_vertexGroups[i].second;
	    }

        // restore color mask
        Renderer::SetColorMask();

        if (bpmem.blendmode.blendenable || bpmem.blendmode.subtract) 
            glEnable(GL_BLEND);
    }

#if defined(_DEBUG) || defined(DEBUGFAST) 
	if (g_Config.iLog & CONF_SAVESHADERS) 
	{
		// save the shaders
		char strfile[255];
		sprintf(strfile, "%sframes/ps%.3d.txt", FULL_DUMP_DIR, g_Config.iSaveTargetId);
		std::ofstream fps(strfile);
		fps << ps->strprog.c_str();
		sprintf(strfile, "%sframes/vs%.3d.txt", FULL_DUMP_DIR, g_Config.iSaveTargetId);
		std::ofstream fvs(strfile);
		fvs << vs->strprog.c_str();
	}

	if (g_Config.iLog & CONF_SAVETARGETS) 
	{
		char str[128];
		sprintf(str, "%sframes/targ%.3d.tga", FULL_DUMP_DIR, g_Config.iSaveTargetId);
		Renderer::SaveRenderTarget(str, Renderer::GetTargetWidth(), Renderer::GetTargetHeight());
	}
#endif
	g_Config.iSaveTargetId++;

	GL_REPORT_ERRORD();

	ResetBuffer();
}

}  // namespace
