// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Globals.h"

#include <fstream>
#include <vector>

#include "Fifo.h"

#include "DriverDetails.h"
#include "VideoConfig.h"
#include "Statistics.h"
#include "MemoryUtil.h"
#include "Render.h"
#include "ImageWrite.h"
#include "BPMemory.h"
#include "TextureCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "ProgramShaderCache.h"
#include "VertexShaderGen.h"
#include "VertexLoader.h"
#include "VertexManager.h"
#include "IndexGenerator.h"
#include "FileUtil.h"
#include "Debugger.h"
#include "StreamBuffer.h"
#include "PerfQueryBase.h"
#include "Render.h"

#include "main.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace OGL
{
//This are the initially requested size for the buffers expressed in bytes
const u32 MAX_IBUFFER_SIZE =  2*1024*1024;
const u32 MAX_VBUFFER_SIZE = 16*1024*1024;

static StreamBuffer *s_vertexBuffer;
static StreamBuffer *s_indexBuffer;
static size_t s_baseVertex;
static size_t s_offset[3];

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
	s_vertexBuffer = new StreamBuffer(GL_ARRAY_BUFFER, MAX_VBUFFER_SIZE);
	m_vertex_buffers = s_vertexBuffer->getBuffer();

	s_indexBuffer = new StreamBuffer(GL_ELEMENT_ARRAY_BUFFER, MAX_IBUFFER_SIZE);
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
	size_t offset = s_vertexBuffer->Upload(GetVertexBuffer(), vertex_data_size);
	s_baseVertex = offset / stride;

	s_indexBuffer->Alloc(index_size);
	if(triangle_index_size)
	{
		s_offset[0] = s_indexBuffer->Upload((u8*)GetTriangleIndexBuffer(), triangle_index_size * sizeof(u16));
	}
	if(line_index_size)
	{
		s_offset[1] = s_indexBuffer->Upload((u8*)GetLineIndexBuffer(), line_index_size * sizeof(u16));
	}
	if(point_index_size)
	{
		s_offset[2] = s_indexBuffer->Upload((u8*)GetPointIndexBuffer(), point_index_size * sizeof(u16));
	}

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertex_data_size);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, index_size);
}

void VertexManager::Draw(u32 stride)
{
	u32 triangle_index_size = IndexGenerator::GetTriangleindexLen();
	u32 line_index_size = IndexGenerator::GetLineindexLen();
	u32 point_index_size = IndexGenerator::GetPointindexLen();
	u32 max_index = IndexGenerator::GetNumVerts();
	GLenum triangle_mode = g_ActiveConfig.backend_info.bSupportsPrimitiveRestart?GL_TRIANGLE_STRIP:GL_TRIANGLES;

	if(g_ogl_config.bSupportsGLBaseVertex) {
		if (triangle_index_size > 0)
		{
			glDrawRangeElementsBaseVertex(triangle_mode, 0, max_index, triangle_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[0], (GLint)s_baseVertex);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);
		}
		if (line_index_size > 0)
		{
			glDrawRangeElementsBaseVertex(GL_LINES, 0, max_index, line_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[1], (GLint)s_baseVertex);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);
		}
		if (point_index_size > 0)
		{
			glDrawRangeElementsBaseVertex(GL_POINTS, 0, max_index, point_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[2], (GLint)s_baseVertex);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);
		}
	} else {
		if (triangle_index_size > 0)
		{
			glDrawRangeElements(triangle_mode, 0, max_index, triangle_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[0]);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);
		}
		if (line_index_size > 0)
		{
			glDrawRangeElements(GL_LINES, 0, max_index, line_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[1]);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);
		}
		if (point_index_size > 0)
		{
			glDrawRangeElements(GL_POINTS, 0, max_index, point_index_size, GL_UNSIGNED_SHORT, (u8*)NULL+s_offset[2]);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);
		}
	}
}

void VertexManager::vFlush()
{
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
			TextureCache::SetNextStage(i);
			g_renderer->SetSamplerState(i % 4, i / 4);
			FourTexUnits &tex = bpmem.tex[i >> 2];
			TextureCache::TCacheEntryBase* tentry = TextureCache::Load(i,
				(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9,
				tex.texTlut[i&3].tlut_format,
				(0 != (tex.texMode0[i&3].min_filter & 3)),
				(tex.texMode1[i&3].max_lod + 0xf) / 0x10,
				(0 != tex.texImage1[i&3].image_type));

			if (tentry)
			{
				// 0s are probably for no manual wrapping needed.
				PixelShaderManager::SetTexDims(i, tentry->native_width, tentry->native_height, 0, 0);
			}
			else
				ERROR_LOG(VIDEO, "Error loading texture");
		}
	}

	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate
		&& bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;

	// Makes sure we can actually do Dual source blending
	bool dualSourcePossible = g_ActiveConfig.backend_info.bSupportsDualSourceBlend;

	// finally bind
	if (dualSourcePossible)
	{
		if (useDstAlpha)
		{
			// If host supports GL_ARB_blend_func_extended, we can do dst alpha in
			// the same pass as regular rendering.
			ProgramShaderCache::SetShader(DSTALPHA_DUAL_SOURCE_BLEND, g_nativeVertexFmt->m_components);
		}
		else
		{
			ProgramShaderCache::SetShader(DSTALPHA_NONE,g_nativeVertexFmt->m_components);
		}
	}
	else
	{
		ProgramShaderCache::SetShader(DSTALPHA_NONE,g_nativeVertexFmt->m_components);
	}

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants(g_nativeVertexFmt->m_components);
	ProgramShaderCache::UploadConstants();

	// setup the pointers
	if (g_nativeVertexFmt)
		g_nativeVertexFmt->SetupVertexPointers();
	GL_REPORT_ERRORD();

	g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
	Draw(stride);
	g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
	//ERROR_LOG(VIDEO, "PerfQuery result: %d", g_perf_query->GetQueryResult(bpmem.zcontrol.early_ztest ? PQ_ZCOMP_OUTPUT_ZCOMPLOC : PQ_ZCOMP_OUTPUT));

	// run through vertex groups again to set alpha
	if (useDstAlpha && !dualSourcePossible)
	{
		ProgramShaderCache::SetShader(DSTALPHA_ALPHA_PASS,g_nativeVertexFmt->m_components);
		if (!g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		{
			// Need to set these again, if we don't support UBO
			VertexShaderManager::SetConstants();
			PixelShaderManager::SetConstants(g_nativeVertexFmt->m_components);
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

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS)
	{
		// save the shaders
		ProgramShaderCache::PCacheEntry prog = ProgramShaderCache::GetShaderProgram();
		char strfile[255];
		sprintf(strfile, "%sps%.3d.txt", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), g_ActiveConfig.iSaveTargetId);
		std::ofstream fps;
		OpenFStream(fps, strfile, std::ios_base::out);
		fps << prog.shader.strpprog.c_str();
		sprintf(strfile, "%svs%.3d.txt", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), g_ActiveConfig.iSaveTargetId);
		std::ofstream fvs;
		OpenFStream(fvs, strfile, std::ios_base::out);
		fvs << prog.shader.strvprog.c_str();
	}

	if (g_ActiveConfig.iLog & CONF_SAVETARGETS)
	{
		char str[128];
		sprintf(str, "%starg%.3d.png", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), g_ActiveConfig.iSaveTargetId);
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
