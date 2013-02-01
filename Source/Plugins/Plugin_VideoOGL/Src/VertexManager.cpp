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
#include "Render.h"
#include "ImageWrite.h"
#include "BPMemory.h"
#include "TextureCache.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexShaderManager.h"
#include "ProgramShaderCache.h"
#include "VertexShaderGen.h"
#include "VertexLoader.h"
#include "VertexManager.h"
#include "IndexGenerator.h"
#include "OpcodeDecoding.h"
#include "FileUtil.h"
#include "Debugger.h"
#include "StreamBuffer.h"

#include "main.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace OGL
{
//This are the initially requeted size for the buffers expresed in bytes
const u32 MAX_IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * 16 * sizeof(u16);
const u32 MAX_VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 16;
const u32 MIN_IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE *  1 * sizeof(u16);
const u32 MIN_VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE *  1;

static StreamBuffer *s_vertexBuffer;
static StreamBuffer *s_indexBuffer;
static u32 s_baseVertex;
static u32 s_offset[3];

VertexManager::VertexManager()
{
	CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
	DestroyDeviceObjects();
}

void VertexManager::CreateDeviceObjects()
{
	GL_REPORT_ERRORD();
	u32 max_Index_size = 0;
	u32 max_Vertex_size = 0;
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*)&max_Index_size);
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, (GLint*)&max_Vertex_size);
	max_Index_size *= sizeof(u16);
	GL_REPORT_ERROR();
	
	u32 index_buffer_size = std::min(MAX_IBUFFER_SIZE, std::max(max_Index_size, MIN_IBUFFER_SIZE));
	u32 vertex_buffer_size = std::min(MAX_VBUFFER_SIZE, std::max(max_Vertex_size, MIN_VBUFFER_SIZE));
	
	// should be not bigger, but we need it. so try and have luck
	if (index_buffer_size > max_Index_size) {
		ERROR_LOG(VIDEO, "GL_MAX_ELEMENTS_INDICES to small, so try it anyway. good luck\n");
	}
	if (vertex_buffer_size > max_Vertex_size) {
		ERROR_LOG(VIDEO, "GL_MAX_ELEMENTS_VERTICES to small, so try it anyway. good luck\n");
	}

	s_vertexBuffer = new StreamBuffer(GL_ARRAY_BUFFER, vertex_buffer_size);
	m_vertex_buffers = s_vertexBuffer->getBuffer();
	s_indexBuffer = new StreamBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_size);
	m_index_buffers = s_indexBuffer->getBuffer();
	
	m_CurrentVertexFmt = NULL;
	m_last_vao = 0;
}
void VertexManager::DestroyDeviceObjects()
{
	GL_REPORT_ERRORD();
	glBindBuffer(GL_ARRAY_BUFFER, 0 );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0 );
	GL_REPORT_ERROR();
	
	delete s_vertexBuffer;
	delete s_indexBuffer;
	GL_REPORT_ERROR();
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	u32 vertex_data_size = IndexGenerator::GetNumVerts() * stride;
	u32 triangle_index_size = IndexGenerator::GetTriangleindexLen();
	u32 line_index_size = IndexGenerator::GetLineindexLen();
	u32 point_index_size = IndexGenerator::GetPointindexLen();
	u32 index_size = (triangle_index_size+line_index_size+point_index_size) * sizeof(u16);
	
	s_vertexBuffer->Alloc(vertex_data_size, stride);
	u32 offset = s_vertexBuffer->Upload(LocalVBuffer, vertex_data_size);
	s_baseVertex = offset / stride;

	s_indexBuffer->Alloc(index_size);
	if(triangle_index_size)
	{
		s_offset[0] = s_indexBuffer->Upload((u8*)TIBuffer, triangle_index_size * sizeof(u16));
	}
	if(line_index_size)
	{
		s_offset[1] = s_indexBuffer->Upload((u8*)LIBuffer, line_index_size * sizeof(u16));
	}
	if(point_index_size)
	{
		s_offset[2] = s_indexBuffer->Upload((u8*)PIBuffer, point_index_size * sizeof(u16));
	}
}

void VertexManager::Draw(u32 stride)
{
	u32 triangle_index_size = IndexGenerator::GetTriangleindexLen();
	u32 line_index_size = IndexGenerator::GetLineindexLen();
	u32 point_index_size = IndexGenerator::GetPointindexLen();
	if (triangle_index_size > 0)
	{
		glDrawElementsBaseVertex(GL_TRIANGLES, triangle_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[0], s_baseVertex);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (line_index_size > 0)
	{
		glDrawElementsBaseVertex(GL_LINES, line_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[1], s_baseVertex);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (point_index_size > 0)
	{
		glDrawElementsBaseVertex(GL_POINTS, point_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[2], s_baseVertex);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
}

void VertexManager::vFlush()
{
	VideoFifo_CheckEFBAccess();
#if defined(_DEBUG) || defined(DEBUGFAST) 
	PRIM_LOG("frame%d:\n texgen=%d, numchan=%d, dualtex=%d, ztex=%d, cole=%d, alpe=%d, ze=%d", g_ActiveConfig.iSaveTargetId, xfregs.numTexGen.numTexGens,
		xfregs.numChan.numColorChans, xfregs.dualTexTrans.enabled, bpmem.ztex2.op,
		bpmem.blendmode.colorupdate, bpmem.blendmode.alphaupdate, bpmem.zmode.updateenable);

	for (unsigned int i = 0; i < xfregs.numChan.numColorChans; ++i) 
	{
		LitChannel* ch = &xfregs.color[i];
		PRIM_LOG("colchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
		ch = &xfregs.alpha[i];
		PRIM_LOG("alpchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
	}

	for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i) 
	{
		TexMtxInfo tinfo = xfregs.texMtxInfo[i];
		if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP) tinfo.hex &= 0x7ff;
		if (tinfo.texgentype != XF_TEXGEN_REGULAR) tinfo.projection = 0;

		PRIM_LOG("txgen%d: proj=%d, input=%d, gentype=%d, srcrow=%d, embsrc=%d, emblght=%d, postmtx=%d, postnorm=%d",
			i, tinfo.projection, tinfo.inputform, tinfo.texgentype, tinfo.sourcerow, tinfo.embosssourceshift, tinfo.embosslightshift,
			xfregs.postMtxInfo[i].index, xfregs.postMtxInfo[i].normalize);
	}

	PRIM_LOG("pixel: tev=%d, ind=%d, texgen=%d, dstalpha=%d, alphatest=0x%x", bpmem.genMode.numtevstages+1, bpmem.genMode.numindstages,
		bpmem.genMode.numtexgens, (u32)bpmem.dstalpha.enable, (bpmem.alpha_test.hex>>16)&0xff);
#endif

	(void)GL_REPORT_ERROR();
	
	GLVertexFormat *nativeVertexFmt = (GLVertexFormat*)g_nativeVertexFmt;
	u32 stride  = nativeVertexFmt->GetVertexStride();
	
	if(m_last_vao != nativeVertexFmt->VAO) {
		glBindVertexArray(nativeVertexFmt->VAO);
		m_last_vao = nativeVertexFmt->VAO;
	}

	PrepareDrawBuffers(stride);
	GL_REPORT_ERRORD();

	u32 usedtextures = 0;
	for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
		if (bpmem.tevorders[i / 2].getEnable(i & 1))
			usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);

	if (bpmem.genMode.numindstages > 0)
		for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);

	for (u32 i = 0; i < 8; i++)
	{
		if (usedtextures & (1 << i))
		{
			FourTexUnits &tex = bpmem.tex[i >> 2];
			TextureCache::TCacheEntryBase* tentry = TextureCache::Load(i, 
				(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
				tex.texTlut[i&3].tlut_format,
				(tex.texMode0[i&3].min_filter & 3) && (tex.texMode0[i&3].min_filter != 8),
				tex.texMode1[i&3].max_lod >> 4,
				tex.texImage1[i&3].image_type);

			if (tentry)
			{
				// 0s are probably for no manual wrapping needed.
				PixelShaderManager::SetTexDims(i, tentry->native_width, tentry->native_height, 0, 0);
			}
			else
				ERROR_LOG(VIDEO, "error loading texture");
		}
	}

	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate
		&& bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;

	// Makes sure we can actually do Dual source blending
	bool dualSourcePossible = g_ActiveConfig.backend_info.bSupportsDualSourceBlend;

	// finally bind
	FRAGMENTSHADER* ps;
	if (dualSourcePossible)
	{
		if (useDstAlpha)
		{
			// If host supports GL_ARB_blend_func_extended, we can do dst alpha in
			// the same pass as regular rendering.
			ps = PixelShaderCache::SetShader(DSTALPHA_DUAL_SOURCE_BLEND, g_nativeVertexFmt->m_components);
		}
		else
		{
			ps = PixelShaderCache::SetShader(DSTALPHA_NONE,g_nativeVertexFmt->m_components);
		}
	}
	else
	{
		ps = PixelShaderCache::SetShader(DSTALPHA_NONE,g_nativeVertexFmt->m_components);
	}
	VERTEXSHADER* vs = VertexShaderCache::SetShader(g_nativeVertexFmt->m_components);
	
	if(ps && vs)
		ProgramShaderCache::SetBothShaders(ps->glprogid, vs->glprogid);

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();
	ProgramShaderCache::UploadConstants();
	
	// setup the pointers
	if (g_nativeVertexFmt)
		g_nativeVertexFmt->SetupVertexPointers();
	GL_REPORT_ERRORD();

	Draw(stride);

	// run through vertex groups again to set alpha
	if (useDstAlpha && !dualSourcePossible)
	{
		ps = PixelShaderCache::SetShader(DSTALPHA_ALPHA_PASS,g_nativeVertexFmt->m_components);
		if(ps && vs)
			ProgramShaderCache::SetBothShaders(ps->glprogid, vs->glprogid);
		if (!g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		{
			// Need to set these again, if we don't support UBO
			VertexShaderManager::SetConstants();
			PixelShaderManager::SetConstants();
		}
	
		// only update alpha
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

		glDisable(GL_BLEND);

		Draw(stride);
		
		// restore color mask
		g_renderer->SetColorMask();

		if (bpmem.blendmode.blendenable || bpmem.blendmode.subtract) 
			glEnable(GL_BLEND);
	}
	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);
	
	ResetBuffer();
#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS) 
	{
		// save the shaders
		char strfile[255];
		sprintf(strfile, "%sps%.3d.txt", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), g_ActiveConfig.iSaveTargetId);
		std::ofstream fps(strfile);
		fps << ps->strprog.c_str();
		sprintf(strfile, "%svs%.3d.txt", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), g_ActiveConfig.iSaveTargetId);
		std::ofstream fvs(strfile);
		fvs << vs->strprog.c_str();
	}

	if (g_ActiveConfig.iLog & CONF_SAVETARGETS) 
	{
		char str[128];
		sprintf(str, "%starg%.3d.tga", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), g_ActiveConfig.iSaveTargetId);
		TargetRectangle tr;
		tr.left = 0;
		tr.right = Renderer::GetTargetWidth();
		tr.top = 0;
		tr.bottom = Renderer::GetTargetHeight();
		g_renderer->SaveScreenshot(str, tr);
	}
#endif
	g_Config.iSaveTargetId++;

	ClearEFBCache();

	GL_REPORT_ERRORD();
}

}  // namespace
