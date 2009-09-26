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

#define MAX_BUFFER_SIZE 0x50000

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace VertexManager
{

static const GLenum c_RenderprimitiveType[8] =
{
    GL_TRIANGLES,
    GL_ZERO, //nothing
    GL_TRIANGLES,
    GL_TRIANGLES,
    GL_TRIANGLES,
    GL_LINES,
    GL_LINES,
    GL_POINTS
};

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

static IndexGenerator indexGen;

static GLenum lastPrimitive;
static GLenum CurrentRenderPrimitive;

static u8 *LocalVBuffer;   
static u16 *IBuffer;  

#define MAXVBUFFERSIZE 0x50000
#define MAXIBUFFERSIZE 0x20000
#define MAXVBOBUFFERCOUNT 0x4

static GLuint s_vboBuffers[MAXVBOBUFFERCOUNT] = {0};
static GLuint s_IBuffers[MAXVBOBUFFERCOUNT] = {0};
static int s_nCurVBOIndex = 0; // current free buffer



bool Init()
{
	lastPrimitive = GL_ZERO;
	CurrentRenderPrimitive = GL_ZERO;
	LocalVBuffer = new u8[MAXVBUFFERSIZE];
	IBuffer = new u16[MAXIBUFFERSIZE];
	s_pCurBufferPointer = LocalVBuffer;
	s_nCurVBOIndex = 0;
	glGenBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	for (u32 i = 0; i < ARRAYSIZE(s_vboBuffers); ++i) {
		glBindBuffer(GL_ARRAY_BUFFER, s_vboBuffers[i]);
		glBufferData(GL_ARRAY_BUFFER, MAXVBUFFERSIZE, NULL, GL_STREAM_DRAW);
	}
	glEnableClientState(GL_VERTEX_ARRAY);
	g_nativeVertexFmt = NULL;
	GL_REPORT_ERRORD();
	return true;
}

void Shutdown()
{
	delete [] LocalVBuffer;
	delete [] IBuffer;
	glDeleteBuffers(ARRAYSIZE(s_vboBuffers), s_vboBuffers);
	s_nCurVBOIndex = 0;
	ResetBuffer();
}

void ResetBuffer()
{
	s_nCurVBOIndex = (s_nCurVBOIndex + 1) % ARRAYSIZE(s_vboBuffers);
	s_pCurBufferPointer = LocalVBuffer;
	CurrentRenderPrimitive = GL_ZERO;
	u16 *ptr = 0;
	indexGen.Start((unsigned short*)ptr);
}

void AddIndices(int _primitive, int _numVertices)
{
	switch (_primitive)
	{
	case GL_QUADS:          indexGen.AddQuads(_numVertices);     return;    
	case GL_TRIANGLES:      indexGen.AddList(_numVertices);      return;    
	case GL_TRIANGLE_STRIP: indexGen.AddStrip(_numVertices);     return;
	case GL_TRIANGLE_FAN:   indexGen.AddFan(_numVertices);       return;
	case GL_LINE_STRIP:     indexGen.AddLineStrip(_numVertices); return;
	case GL_LINES:		     indexGen.AddLineList(_numVertices);  return;
	case GL_POINTS:         indexGen.AddPoints(_numVertices);    return;
	}
}

int GetRemainingSize()
{
	return LocalVBuffer + MAXVBUFFERSIZE - s_pCurBufferPointer;
}

void AddVertices(int primitive, int numvertices)
{
	if (numvertices <= 0)
		return;

	if (c_primitiveType[primitive] == GL_ZERO)
		return;
	DVSTARTPROFILE();	
	
	lastPrimitive = c_primitiveType[primitive];
	
	ADDSTAT(stats.thisFrame.numPrims, numvertices);

	if (CurrentRenderPrimitive != c_RenderprimitiveType[primitive])
	{
		// We are NOT collecting the right type.
		Flush();
		CurrentRenderPrimitive = c_RenderprimitiveType[primitive];
		u16 *ptr = 0;
		if (lastPrimitive != GL_POINTS)
		{
			ptr = IBuffer;			
		}
		indexGen.Start((unsigned short*)ptr);
		AddIndices(c_primitiveType[primitive], numvertices);		
	}
	else  // We are collecting the right type, keep going
	{
		INCSTAT(stats.thisFrame.numPrimitiveJoins);
		AddIndices(c_primitiveType[primitive], numvertices);		
	}
}

inline void Draw(int numVertices, int indexLen)
{	

	if (CurrentRenderPrimitive != GL_POINT)
	{		
		glDrawElements(CurrentRenderPrimitive, indexLen, GL_UNSIGNED_SHORT, IBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	else
	{
		glDrawArrays(CurrentRenderPrimitive,0,numVertices);
		INCSTAT(stats.thisFrame.numDrawCalls);
	}
	
}

void Flush()
{
	if (LocalVBuffer == s_pCurBufferPointer) return;	
	int numVerts = indexGen.GetNumVerts();
	if(numVerts == 0) return;

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

	GL_REPORT_ERRORD(); 

	
	
	glBindBuffer(GL_ARRAY_BUFFER, s_vboBuffers[s_nCurVBOIndex]);
	glBufferSubData(GL_ARRAY_BUFFER,0, s_pCurBufferPointer - LocalVBuffer, LocalVBuffer);
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

	u32 nonpow2tex = 0;
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
				// texture loaded fine, set dims for pixel shader
				if (tentry->isRectangle) 
				{
					PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, tentry->mode.wrap_s, tentry->mode.wrap_t);
					nonpow2tex |= 1 << i;
					if (tentry->mode.wrap_s > 0) nonpow2tex |= 1 << (8 + i);
					if (tentry->mode.wrap_t > 0) nonpow2tex |= 1 << (16 + i);
				}
				// if texture is power of two, set to ones (since don't need scaling)
				// (the above seems to have changed - we set the width and height here too.
				else 
				{
					// 0s are probably for no manual wrapping needed.
					PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, 0, 0);
				}
				// texture is hires - pass the scaling size
				if (tentry->scaleX != 1.0f || tentry->scaleY != 1.0f)
					PixelShaderManager::SetCustomTexScale(i, tentry->scaleX, tentry->scaleY);
				if (g_ActiveConfig.iLog & CONF_SAVETEXTURES) 
				{
					// save the textures
					char strfile[255];
					sprintf(strfile, "%sframes/tex%.3d_%d.tga", FULL_DUMP_DIR, g_Config.iSaveTargetId, i);
					SaveTexture(strfile, tentry->isRectangle?GL_TEXTURE_RECTANGLE_ARB:GL_TEXTURE_2D, tentry->texture, tentry->w, tentry->h);
				}
			}
			else
				ERROR_LOG(VIDEO, "error loading tex\n");
		}
	}

	PixelShaderManager::SetTexturesUsed(nonpow2tex);

	FRAGMENTSHADER* ps = PixelShaderCache::GetShader(false);
	VERTEXSHADER* vs = VertexShaderCache::GetShader(g_nativeVertexFmt->m_components);

	// set global constants
    VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	// finally bind

	int groupStart = 0;
	if (vs) VertexShaderCache::SetCurrentShader(vs->glprogid);
	if (ps) PixelShaderCache::SetCurrentShader(ps->glprogid); // Lego Star Wars crashes here.

#if defined(_DEBUG) || defined(DEBUGFAST) 
	PRIM_LOG("");
#endif

	int numIndexes = indexGen.GetindexLen();
	Draw(numVerts,numIndexes);
	
    // run through vertex groups again to set alpha
    if (!g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate) 
	{
        ps = PixelShaderCache::GetShader(true);

        if (ps)PixelShaderCache::SetCurrentShader(ps->glprogid);

		// only update alpha
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

		glDisable(GL_BLEND);

		Draw(numVerts,numIndexes);
		// restore color mask
		Renderer::SetColorMask();

		if (bpmem.blendmode.blendenable || bpmem.blendmode.subtract) 
			glEnable(GL_BLEND);	
    }

#if defined(_DEBUG) || defined(DEBUGFAST) 
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS) 
	{
		// save the shaders
		char strfile[255];
		sprintf(strfile, "%sframes/ps%.3d.txt", FULL_DUMP_DIR, g_ActiveConfig.iSaveTargetId);
		std::ofstream fps(strfile);
		fps << ps->strprog.c_str();
		sprintf(strfile, "%sframes/vs%.3d.txt", FULL_DUMP_DIR, g_ActiveConfig.iSaveTargetId);
		std::ofstream fvs(strfile);
		fvs << vs->strprog.c_str();
	}

	if (g_ActiveConfig.iLog & CONF_SAVETARGETS) 
	{
		char str[128];
		sprintf(str, "%sframes/targ%.3d.tga", FULL_DUMP_DIR, g_ActiveConfig.iSaveTargetId);
		Renderer::SaveRenderTarget(str, Renderer::GetTargetWidth(), Renderer::GetTargetHeight());
	}
#endif
	g_Config.iSaveTargetId++;

	GL_REPORT_ERRORD();

	ResetBuffer();
}
}  // namespace

