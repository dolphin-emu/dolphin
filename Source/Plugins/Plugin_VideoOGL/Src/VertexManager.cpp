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
#include "VertexShaderGen.h"
#include "VertexLoader.h"
#include "VertexManager.h"
#include "IndexGenerator.h"
#include "OpcodeDecoding.h"
#include "FileUtil.h"
#include "Debugger.h"

#include "main.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace OGL
{
//This are the initially requeted size for the buffers expresed in bytes
const u32 IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * 16 * sizeof(u16);
const u32 VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 16;
const u32 MAX_VBUFFER_COUNT = 2;

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
	m_buffers_count = 0;
	m_vertex_buffers = NULL;
	m_index_buffers = NULL;
	glEnableClientState(GL_VERTEX_ARRAY);
	GL_REPORT_ERRORD();
	int max_Index_size = 0;
	int max_Vertex_size = 0;
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*)&max_Index_size);
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, (GLint*)&max_Vertex_size);
	max_Index_size *= sizeof(u16);
	GL_REPORT_ERROR();
	m_index_buffer_size = IBUFFER_SIZE;
	if (max_Index_size > 0 && max_Index_size < m_index_buffer_size)
		m_index_buffer_size = max_Index_size;

	m_vertex_buffer_size = VBUFFER_SIZE;
	if (max_Vertex_size > 0 && max_Vertex_size < m_vertex_buffer_size)
		m_vertex_buffer_size = max_Vertex_size;

	if (m_index_buffer_size < VertexManager::MAXIBUFFERSIZE || m_vertex_buffer_size < VertexManager::MAXVBUFFERSIZE)
	{
		return;
	}

	m_vertex_buffers = new GLuint[MAX_VBUFFER_COUNT];
	m_index_buffers = new GLuint[MAX_VBUFFER_COUNT];

	glGenBuffers(MAX_VBUFFER_COUNT, m_vertex_buffers);
	GL_REPORT_ERROR();
	glGenBuffers(MAX_VBUFFER_COUNT, m_index_buffers);
	GL_REPORT_ERROR();
	for (u32 i = 0; i < MAX_VBUFFER_COUNT; i++)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffers[i] );
		GL_REPORT_ERROR();
		glBufferData(GL_ARRAY_BUFFER, m_vertex_buffer_size, NULL, GL_STREAM_DRAW  );
		GL_REPORT_ERROR();
	}
	for (u32 i = 0; i < MAX_VBUFFER_COUNT; i++)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffers[i] );
		GL_REPORT_ERROR();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer_size, NULL, GL_STREAM_DRAW  );
		GL_REPORT_ERROR();
	}
	m_buffers_count = MAX_VBUFFER_COUNT;
	m_current_index_buffer = 0;
	m_current_vertex_buffer = 0;
	m_index_buffer_cursor = m_index_buffer_size;
	m_vertex_buffer_cursor = m_vertex_buffer_size;
	m_CurrentVertexFmt = NULL;
}
void VertexManager::DestroyDeviceObjects()
{
	glDisableClientState(GL_VERTEX_ARRAY);
	GL_REPORT_ERRORD();
	glBindBuffer(GL_ARRAY_BUFFER, NULL );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, NULL );
	GL_REPORT_ERROR();
	if(m_vertex_buffers)
	{
		glDeleteBuffers(MAX_VBUFFER_COUNT, m_vertex_buffers);
		GL_REPORT_ERROR();
		delete [] m_vertex_buffers;
	}
	if(m_index_buffers)
	{
		glDeleteBuffers(MAX_VBUFFER_COUNT, m_index_buffers);
		GL_REPORT_ERROR();
		delete [] m_index_buffers;
	}
	m_vertex_buffers = NULL;
	m_index_buffers = NULL;
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	if (!m_buffers_count)
	{
		return;
	}
	u8* pVertices = NULL;
	u16* pIndices = NULL;
	int vertex_data_size = IndexGenerator::GetNumVerts() * stride;
	int triangle_index_size = IndexGenerator::GetTriangleindexLen();
	int line_index_size = IndexGenerator::GetLineindexLen();
	int point_index_size = IndexGenerator::GetPointindexLen();
	int index_data_size = (triangle_index_size + line_index_size + point_index_size) * sizeof(u16);
	GLbitfield LockMode = GL_MAP_WRITE_BIT;	
	m_vertex_buffer_cursor--;
	m_vertex_buffer_cursor = m_vertex_buffer_cursor - (m_vertex_buffer_cursor % stride) + stride;
	if (m_vertex_buffer_cursor > m_vertex_buffer_size - vertex_data_size)
	{
		LockMode |= GL_MAP_INVALIDATE_BUFFER_BIT;
		m_vertex_buffer_cursor = 0;
		m_current_vertex_buffer = (m_current_vertex_buffer + 1) % m_buffers_count;
		glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffers[m_current_vertex_buffer]);
	}
	else
	{
		LockMode |= GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
	}	
	if(GLEW_ARB_map_buffer_range)
	{
		pVertices = (u8*)glMapBufferRange(GL_ARRAY_BUFFER, m_vertex_buffer_cursor, vertex_data_size, LockMode);
		if(pVertices)
		{
			memcpy(pVertices, LocalVBuffer, vertex_data_size);
			glUnmapBuffer(GL_ARRAY_BUFFER);
		}
		else
		{
			glBufferSubData(GL_ARRAY_BUFFER, m_vertex_buffer_cursor, vertex_data_size, LocalVBuffer);
		}		
	}
	else
	{
		glBufferSubData(GL_ARRAY_BUFFER, m_vertex_buffer_cursor, vertex_data_size, LocalVBuffer);
	}

	LockMode = GL_MAP_WRITE_BIT;

	if (m_index_buffer_cursor > m_index_buffer_size - index_data_size)
	{
		LockMode |= GL_MAP_INVALIDATE_BUFFER_BIT;
		m_index_buffer_cursor = 0;
		m_current_index_buffer = (m_current_index_buffer + 1) % m_buffers_count;		
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffers[m_current_index_buffer]);
	}
	else
	{
		LockMode |= GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
	}	
	if(GLEW_ARB_map_buffer_range)
	{
		pIndices = (u16*)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer_cursor , index_data_size, LockMode);
		if(pIndices)
		{
			if(triangle_index_size)
			{		
				memcpy(pIndices, TIBuffer, triangle_index_size * sizeof(u16));
				pIndices += triangle_index_size;
			}
			if(line_index_size)
			{		
				memcpy(pIndices, LIBuffer, line_index_size * sizeof(u16));
				pIndices += line_index_size;
			}
			if(point_index_size)
			{		
				memcpy(pIndices, PIBuffer, point_index_size * sizeof(u16));
			}
			glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		}
		else
		{
			if(triangle_index_size)
			{		
				triangle_index_size *= sizeof(u16);
				glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,  m_index_buffer_cursor, triangle_index_size, TIBuffer);				
			}
			if(line_index_size)
			{
				line_index_size *= sizeof(u16);
				glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,  m_index_buffer_cursor + triangle_index_size, line_index_size, LIBuffer);
			}
			if(point_index_size)
			{
				point_index_size *= sizeof(u16);
				glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,  m_index_buffer_cursor + triangle_index_size + line_index_size, point_index_size, PIBuffer);
			}			
		}
	}	
}

void VertexManager::DrawVertexArray()
{
	int triangle_index_size = IndexGenerator::GetTriangleindexLen();
	int line_index_size = IndexGenerator::GetLineindexLen();
	int point_index_size = IndexGenerator::GetPointindexLen();
	if (triangle_index_size > 0)
	{
		glDrawElements(GL_TRIANGLES, triangle_index_size, GL_UNSIGNED_SHORT, TIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (line_index_size > 0)
	{
		glDrawElements(GL_LINES, line_index_size, GL_UNSIGNED_SHORT, LIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (point_index_size > 0)
	{
		glDrawElements(GL_POINTS, IndexGenerator::GetPointindexLen(), GL_UNSIGNED_SHORT, PIBuffer);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
}

void VertexManager::DrawVertexBufferObject()
{
	int triangle_index_size = IndexGenerator::GetTriangleindexLen();
	int line_index_size = IndexGenerator::GetLineindexLen();
	int point_index_size = IndexGenerator::GetPointindexLen();
	int StartIndex = m_index_buffer_cursor;
	if (triangle_index_size > 0)
	{
		glDrawElements(GL_TRIANGLES, triangle_index_size, GL_UNSIGNED_SHORT, (GLvoid*)StartIndex);
		StartIndex += triangle_index_size * sizeof(u16);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (line_index_size > 0)
	{		
		glDrawElements(GL_LINES, line_index_size, GL_UNSIGNED_SHORT, (GLvoid*)StartIndex);
		StartIndex += line_index_size * sizeof(u16);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (point_index_size > 0)
	{		
		glDrawElements(GL_POINTS, point_index_size, GL_UNSIGNED_SHORT, (GLvoid*)StartIndex);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
}

void VertexManager::DrawVertexBufferObjectBase(u32 stride)
{
	int triangle_index_size = IndexGenerator::GetTriangleindexLen();
	int line_index_size = IndexGenerator::GetLineindexLen();
	int point_index_size = IndexGenerator::GetPointindexLen();
	int StartIndex = m_index_buffer_cursor;
	int basevertex = m_vertex_buffer_cursor / stride;
	if (triangle_index_size > 0)
	{
		glDrawElementsBaseVertex(GL_TRIANGLES, triangle_index_size, GL_UNSIGNED_SHORT, (GLvoid*)StartIndex, basevertex);
		StartIndex += triangle_index_size * sizeof(u16);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (line_index_size > 0)
	{
		glDrawElementsBaseVertex(GL_LINES, line_index_size, GL_UNSIGNED_SHORT, (GLvoid*)StartIndex, basevertex);
		StartIndex += line_index_size * sizeof(u16);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (point_index_size > 0)
	{
		glDrawElementsBaseVertex(GL_POINTS, point_index_size, GL_UNSIGNED_SHORT, (GLvoid*)StartIndex, basevertex);
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

	PRIM_LOG("pixel: tev=%d, ind=%d, texgen=%d, dstalpha=%d, alphafunc=0x%x", bpmem.genMode.numtevstages+1, bpmem.genMode.numindstages,
		bpmem.genMode.numtexgens, (u32)bpmem.dstalpha.enable, (bpmem.alphaFunc.hex>>16)&0xff);
#endif

	(void)GL_REPORT_ERROR();
	
	u32 stride  = g_nativeVertexFmt->GetVertexStride();

	PrepareDrawBuffers(stride);
	//still testing if this line is enabled to reduce the amount of vertex setup call everything goes wrong
	//if(m_CurrentVertexFmt != g_nativeVertexFmt || !GLEW_ARB_draw_elements_base_vertex )
	{
		if(m_buffers_count)
		{
			((GLVertexFormat*)g_nativeVertexFmt)->SetupVertexPointersOffset(GLEW_ARB_draw_elements_base_vertex ? 0 : m_vertex_buffer_cursor);			
		}
		else
		{
			g_nativeVertexFmt->SetupVertexPointers();
		}
		m_CurrentVertexFmt = g_nativeVertexFmt;
	}
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
			glActiveTexture(GL_TEXTURE0 + i);
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

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate
		&& bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;

#ifdef USE_DUAL_SOURCE_BLEND
	bool dualSourcePossible = GLEW_ARB_blend_func_extended;

	// finally bind
	FRAGMENTSHADER* ps;
	if (dualSourcePossible)
	{
		if (useDstAlpha)
		{
			// If host supports GL_ARB_blend_func_extended, we can do dst alpha in
			// the same pass as regular rendering.
			g_renderer->SetBlendMode(true);
			ps = PixelShaderCache::SetShader(DSTALPHA_DUAL_SOURCE_BLEND, g_nativeVertexFmt->m_components);
		}
		else
		{
			g_renderer->SetBlendMode(true);
			ps = PixelShaderCache::SetShader(DSTALPHA_NONE,g_nativeVertexFmt->m_components);
		}
	}
	else
	{
		ps = PixelShaderCache::SetShader(DSTALPHA_NONE,g_nativeVertexFmt->m_components);
	}
#else
	bool dualSourcePossible = false;
	FRAGMENTSHADER* ps = PixelShaderCache::SetShader(DSTALPHA_NONE,g_nativeVertexFmt->m_components);
#endif
	VERTEXSHADER* vs = VertexShaderCache::SetShader(g_nativeVertexFmt->m_components);
	if (ps) PixelShaderCache::SetCurrentShader(ps->glprogid); // Lego Star Wars crashes here.
	if (vs) VertexShaderCache::SetCurrentShader(vs->glprogid);

	if(m_buffers_count) 
	{ 
		if(GLEW_ARB_draw_elements_base_vertex)
		{
			DrawVertexBufferObjectBase(stride);
		} 
		else
		{
			DrawVertexBufferObject();
		} 
	}
	else
	{ 
		DrawVertexArray();
	}

	// run through vertex groups again to set alpha
	if (useDstAlpha && !dualSourcePossible)
	{
		ps = PixelShaderCache::SetShader(DSTALPHA_ALPHA_PASS,g_nativeVertexFmt->m_components);
		if (ps) PixelShaderCache::SetCurrentShader(ps->glprogid);

		// only update alpha
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

		glDisable(GL_BLEND);

		if(m_buffers_count) 
		{
			if(GLEW_ARB_draw_elements_base_vertex)
			{
				DrawVertexBufferObjectBase(stride);
			} 
			else
			{
				DrawVertexBufferObject();
			} 
		}
		else
		{ 
			DrawVertexArray();
		} 
		// restore color mask
		g_renderer->SetColorMask();

		if (bpmem.blendmode.blendenable || bpmem.blendmode.subtract) 
			glEnable(GL_BLEND);
	}
	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);
	if(m_buffers_count)
	{
		m_index_buffer_cursor += (IndexGenerator::GetTriangleindexLen() + IndexGenerator::GetLineindexLen() + IndexGenerator::GetPointindexLen()) * sizeof(u16);
		m_vertex_buffer_cursor += IndexGenerator::GetNumVerts() * stride;
	}
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
