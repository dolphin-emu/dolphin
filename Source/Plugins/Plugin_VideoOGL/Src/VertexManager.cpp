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

#include <fstream>
#include <vector>

#include "Fifo.h"

#include "VideoConfig.h"
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
#include "IndexGenerator.h"
#include "OpcodeDecoding.h"
#include "FileUtil.h"

#include "main.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace VertexManager
{

static int lastPrimitive;

static u8 *LocalVBuffer;
static u16 *TIBuffer;
static u16 *LIBuffer;
static u16 *PIBuffer;
static GLint max_Index_size = 0;
#define MAXVBUFFERSIZE 0x1FFFF
#define MAXIBUFFERSIZE 0xFFFF
#define MAXVBOBUFFERCOUNT 0x8

//static GLuint s_vboBuffers[MAXVBOBUFFERCOUNT] = {0};
//static int s_nCurVBOIndex = 0; // current free buffer
static bool Flushed=false;


bool Init()
{
	lastPrimitive = GX_DRAW_NONE;
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint *)&max_Index_size);
	
	if(max_Index_size>MAXIBUFFERSIZE)
		max_Index_size = MAXIBUFFERSIZE;
	
	LocalVBuffer = new u8[MAXVBUFFERSIZE];
	TIBuffer = new u16[max_Index_size];
	LIBuffer = new u16[max_Index_size];
	PIBuffer = new u16[max_Index_size];
	IndexGenerator::Start(TIBuffer,LIBuffer,PIBuffer);
	s_pCurBufferPointer = LocalVBuffer;
	s_pBaseBufferPointer = LocalVBuffer;
	//s_nCurVBOIndex = 0;
	//glGenBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	glEnableClientState(GL_VERTEX_ARRAY);
	g_nativeVertexFmt = NULL;
	Flushed=false;
	GL_REPORT_ERRORD();
	
	return true;
}

void Shutdown()
{
	delete [] LocalVBuffer;
	delete [] TIBuffer;
	delete [] LIBuffer;
	delete [] PIBuffer;
	//glDeleteBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	//s_nCurVBOIndex = 0;
}

void ResetBuffer()
{
	//s_nCurVBOIndex = (s_nCurVBOIndex + 1) % ARRAYSIZE(s_vboBuffers);
	s_pCurBufferPointer = LocalVBuffer;
}

void AddIndices(int primitive, int numVertices)
{
	switch (primitive)
	{
		case GX_DRAW_QUADS:          IndexGenerator::AddQuads(numVertices);break;    
		case GX_DRAW_TRIANGLES:      IndexGenerator::AddList(numVertices);break;
		case GX_DRAW_TRIANGLE_STRIP: IndexGenerator::AddStrip(numVertices);     break;
		case GX_DRAW_TRIANGLE_FAN:   IndexGenerator::AddFan(numVertices);       break;
		case GX_DRAW_LINE_STRIP:     IndexGenerator::AddLineStrip(numVertices); break;
		case GX_DRAW_LINES:          IndexGenerator::AddLineList(numVertices);break;
		case GX_DRAW_POINTS:         IndexGenerator::AddPoints(numVertices);    break;
	}
}

int GetRemainingSize()
{
	return  MAXVBUFFERSIZE - (int)(s_pCurBufferPointer - LocalVBuffer);
}

int GetRemainingVertices(int primitive)
{
	switch (primitive)
	{
		case GX_DRAW_QUADS:
		case GX_DRAW_TRIANGLES:
		case GX_DRAW_TRIANGLE_STRIP:
		case GX_DRAW_TRIANGLE_FAN:
			return (max_Index_size - IndexGenerator::GetTriangleindexLen())/3;
		case GX_DRAW_LINE_STRIP:
		case GX_DRAW_LINES:
			return (max_Index_size - IndexGenerator::GetLineindexLen())/2;
		case GX_DRAW_POINTS:
			return (max_Index_size - IndexGenerator::GetPointindexLen());
		default: return 0;
	}
}

void AddVertices(int primitive, int numvertices)
{
	if (numvertices <= 0)
		return;
	(void)GL_REPORT_ERROR();
	switch (primitive)
	{
		case GX_DRAW_QUADS:
		case GX_DRAW_TRIANGLES:
		case GX_DRAW_TRIANGLE_STRIP:
		case GX_DRAW_TRIANGLE_FAN:
			if(max_Index_size - IndexGenerator::GetTriangleindexLen() < 3 * numvertices)
				Flush();
			break;
		case GX_DRAW_LINE_STRIP:
		case GX_DRAW_LINES:
			if(max_Index_size - IndexGenerator::GetLineindexLen() < 2 * numvertices)
				Flush();
			break;
		case GX_DRAW_POINTS:
			if(max_Index_size - IndexGenerator::GetPointindexLen() < numvertices)
				Flush();
			break;
		default: return;
	}
	if(Flushed)
	{
		IndexGenerator::Start(TIBuffer,LIBuffer,PIBuffer);
		Flushed=false;
	}
	lastPrimitive = primitive;
	ADDSTAT(stats.thisFrame.numPrims, numvertices);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	AddIndices(primitive, numvertices);
	
	
}

inline void Draw()
{
	if(IndexGenerator::GetNumTriangles() > 0)
	{
		glDrawElements(GL_TRIANGLES, IndexGenerator::GetTriangleindexLen(), GL_UNSIGNED_SHORT, TIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if(IndexGenerator::GetNumLines() > 0)
	{
		glDrawElements(GL_LINES, IndexGenerator::GetLineindexLen(), GL_UNSIGNED_SHORT, LIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if(IndexGenerator::GetNumPoints() > 0)
	{
		glDrawElements(GL_POINTS, IndexGenerator::GetPointindexLen(), GL_UNSIGNED_SHORT, PIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
}

void Flush()
{
	if (LocalVBuffer == s_pCurBufferPointer) return;
	if(Flushed) return;
	Flushed=true;
	VideoFifo_CheckEFBAccess();
#if defined(_DEBUG) || defined(DEBUGFAST) 
	PRIM_LOG("frame%d:\n texgen=%d, numchan=%d, dualtex=%d, ztex=%d, cole=%d, alpe=%d, ze=%d", g_ActiveConfig.iSaveTargetId, xfregs.numTexGens,
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

	(void)GL_REPORT_ERROR();
	
	//glBindBuffer(GL_ARRAY_BUFFER, s_vboBuffers[s_nCurVBOIndex]);
	//glBufferData(GL_ARRAY_BUFFER, s_pCurBufferPointer - LocalVBuffer, LocalVBuffer, GL_STREAM_DRAW);
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

			if (tentry) 
			{
				// 0s are probably for no manual wrapping needed.
				PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, 0, 0);

				if (g_ActiveConfig.iLog & CONF_SAVETEXTURES) 
				{
					// save the textures
					char strfile[255];
					sprintf(strfile, "%stex%.3d_%d.tga", File::GetUserPath(D_DUMPFRAMES_IDX), g_Config.iSaveTargetId, i);
					SaveTexture(strfile, GL_TEXTURE_2D, tentry->texture, tentry->w, tentry->h);
				}
			}
			else
				ERROR_LOG(VIDEO, "error loading tex\n");
		}
	}

	FRAGMENTSHADER* ps = PixelShaderCache::GetShader(false,g_nativeVertexFmt->m_components);
	VERTEXSHADER* vs = VertexShaderCache::GetShader(g_nativeVertexFmt->m_components);

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	// finally bind

	if (vs) VertexShaderCache::SetCurrentShader(vs->glprogid);
	if (ps) PixelShaderCache::SetCurrentShader(ps->glprogid); // Lego Star Wars crashes here.

	Draw();
	
	// run through vertex groups again to set alpha
	if (!g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate) 
	{
		ps = PixelShaderCache::GetShader(true,g_nativeVertexFmt->m_components);

		if (ps)PixelShaderCache::SetCurrentShader(ps->glprogid);

		// only update alpha
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

		glDisable(GL_BLEND);

		Draw();
		// restore color mask
		Renderer::SetColorMask();

		if (bpmem.blendmode.blendenable || bpmem.blendmode.subtract) 
			glEnable(GL_BLEND);
	}
	//s_nCurVBOIndex = (s_nCurVBOIndex + 1) % ARRAYSIZE(s_vboBuffers);
	s_pCurBufferPointer = LocalVBuffer;
	IndexGenerator::Start(TIBuffer,LIBuffer,PIBuffer);

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS) 
	{
		// save the shaders
		char strfile[255];
		sprintf(strfile, "%sps%.3d.txt", File::GetUserPath(D_DUMPFRAMES_IDX), g_ActiveConfig.iSaveTargetId);
		std::ofstream fps(strfile);
		fps << ps->strprog.c_str();
		sprintf(strfile, "%svs%.3d.txt", File::GetUserPath(D_DUMPFRAMES_IDX), g_ActiveConfig.iSaveTargetId);
		std::ofstream fvs(strfile);
		fvs << vs->strprog.c_str();
	}

	if (g_ActiveConfig.iLog & CONF_SAVETARGETS) 
	{
		char str[128];
		sprintf(str, "%starg%.3d.tga", File::GetUserPath(D_DUMPFRAMES_IDX), g_ActiveConfig.iSaveTargetId);
		TargetRectangle tr;
		tr.left = 0;
		tr.right = Renderer::GetTargetWidth();
		tr.top = 0;
		tr.bottom = Renderer::GetTargetHeight();
		Renderer::SaveRenderTarget(str, tr);
	}
#endif
	g_Config.iSaveTargetId++;

	GL_REPORT_ERRORD();

}
}  // namespace

