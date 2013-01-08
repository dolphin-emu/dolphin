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

//static GLint max_Index_size = 0;

//static GLuint s_vboBuffers[MAXVBOBUFFERCOUNT] = {0};
//static int s_nCurVBOIndex = 0; // current free buffer

VertexManager::VertexManager()
{
	// TODO: doesn't seem to be used anywhere

	//glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*)&max_Index_size);
	//
	//if (max_Index_size > MAXIBUFFERSIZE)
	//	max_Index_size = MAXIBUFFERSIZE;
	//
	//GL_REPORT_ERRORD();

	glEnableClientState(GL_VERTEX_ARRAY);
	GL_REPORT_ERRORD();
}

void VertexManager::CreateDeviceObjects()
{
	

}
void VertexManager::DestroyDeviceObjects()
{
	
}

void VertexManager::Draw()
{
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
}

void VertexManager::vFlush()
{
	if (LocalVBuffer == s_pCurBufferPointer) return;
	if (Flushed) return;
	Flushed=true;
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
	
	//glBindBuffer(GL_ARRAY_BUFFER, s_vboBuffers[s_nCurVBOIndex]);
	//glBufferData(GL_ARRAY_BUFFER, s_pCurBufferPointer - LocalVBuffer, LocalVBuffer, GL_STREAM_DRAW);
	GL_REPORT_ERRORD();

	// setup the pointers
	if (g_nativeVertexFmt)
		g_nativeVertexFmt->SetupVertexPointers();
	GL_REPORT_ERRORD();

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

	Draw();

	// run through vertex groups again to set alpha
	if (useDstAlpha && !dualSourcePossible)
	{
		ps = PixelShaderCache::SetShader(DSTALPHA_ALPHA_PASS,g_nativeVertexFmt->m_components);
		if (ps) PixelShaderCache::SetCurrentShader(ps->glprogid);

		// only update alpha
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

		glDisable(GL_BLEND);

		Draw();
		// restore color mask
		g_renderer->SetColorMask();

		if (bpmem.blendmode.blendenable || bpmem.blendmode.subtract) 
			glEnable(GL_BLEND);
	}
	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

	//s_nCurVBOIndex = (s_nCurVBOIndex + 1) % ARRAYSIZE(s_vboBuffers);
	s_pCurBufferPointer = LocalVBuffer;
	IndexGenerator::Start(TIBuffer,LIBuffer,PIBuffer);

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
