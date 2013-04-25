
#include "Common.h"

#include "Statistics.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "NativeVertexFormat.h"
#include "TextureCacheBase.h"
#include "RenderBase.h"
#include "BPStructs.h"

#include "VertexManagerBase.h"
#include "MainBase.h"
#include "VideoConfig.h"

VertexManager *g_vertex_manager;

u8 *VertexManager::s_pCurBufferPointer;
u8 *VertexManager::s_pBaseBufferPointer;
u8 *VertexManager::s_pEndBufferPointer;

VertexManager::VertexManager()
{
	LocalVBuffer.resize(MAXVBUFFERSIZE);
	s_pCurBufferPointer = s_pBaseBufferPointer = &LocalVBuffer[0];
	s_pEndBufferPointer = s_pBaseBufferPointer + LocalVBuffer.size();

	TIBuffer.resize(MAXIBUFFERSIZE);
	LIBuffer.resize(MAXIBUFFERSIZE);
	PIBuffer.resize(MAXIBUFFERSIZE);

	ResetBuffer();
}

VertexManager::~VertexManager()
{
}

void VertexManager::ResetBuffer()
{
	s_pCurBufferPointer = s_pBaseBufferPointer;
	IndexGenerator::Start(GetTriangleIndexBuffer(), GetLineIndexBuffer(), GetPointIndexBuffer());
}

u32 VertexManager::GetRemainingSize()
{
	return (u32)(s_pEndBufferPointer - s_pCurBufferPointer);
}

void VertexManager::PrepareForAdditionalData(int primitive, u32 count, u32 stride)
{	
	u32 const needed_vertex_bytes = count * stride;
	
	if (count > IndexGenerator::GetRemainingIndices() || count > GetRemainingIndices(primitive) || needed_vertex_bytes > GetRemainingSize())
	{
		Flush();
		
		if(count > IndexGenerator::GetRemainingIndices())
			ERROR_LOG(VIDEO, "Too little remaining index values. Use 32-bit or reset them on flush.");
		if (count > GetRemainingIndices(primitive))
			ERROR_LOG(VIDEO, "VertexManager: Buffer not large enough for all indices! "
				"Increase MAXIBUFFERSIZE or we need primitive breaking after all.");
		if (needed_vertex_bytes > GetRemainingSize())
			ERROR_LOG(VIDEO, "VertexManager: Buffer not large enough for all vertices! "
				"Increase MAXVBUFFERSIZE or we need primitive breaking after all.");
	}
}

bool VertexManager::IsFlushed() const
{
	return s_pBaseBufferPointer == s_pCurBufferPointer;
}

u32 VertexManager::GetRemainingIndices(int primitive)
{
	
	if(g_Config.backend_info.bSupportsPrimitiveRestart)
	{
		switch (primitive)
		{
		case GX_DRAW_QUADS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 5 * 4;
		case GX_DRAW_TRIANGLES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 4 * 3;
		case GX_DRAW_TRIANGLE_STRIP:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 1 - 1;
		case GX_DRAW_TRIANGLE_FAN:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 6 * 4 + 1;

		case GX_DRAW_LINES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen());
		case GX_DRAW_LINE_STRIP:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen()) / 2 + 1;

		case GX_DRAW_POINTS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen());

		default:
			return 0;
		}
	}
	else
	{
		switch (primitive)
		{
		case GX_DRAW_QUADS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 6 * 4;
		case GX_DRAW_TRIANGLES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen());
		case GX_DRAW_TRIANGLE_STRIP:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 3 + 2;
		case GX_DRAW_TRIANGLE_FAN:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 3 + 2;

		case GX_DRAW_LINES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen());
		case GX_DRAW_LINE_STRIP:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen()) / 2 + 1;

		case GX_DRAW_POINTS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen());

		default:
			return 0;
		}
	} 
}

void VertexManager::AddVertices(int primitive, u32 numVertices)
{
	if (numVertices <= 0)
		return;

	ADDSTAT(stats.thisFrame.numPrims, numVertices);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);

	IndexGenerator::AddIndices(primitive, numVertices);
}

void VertexManager::Flush()
{
	if (g_vertex_manager->IsFlushed())
		return;

	// loading a state will invalidate BP, so check for it
	g_video_backend->CheckInvalidState();

	VideoFifo_CheckEFBAccess();

	g_vertex_manager->vFlush();

	g_vertex_manager->ResetBuffer();
}

// TODO: need to merge more stuff into VideoCommon to use this
#if (0)
void VertexManager::Flush()
{
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
		bpmem.genMode.numtexgens, (u32)bpmem.dstalpha.enable, (bpmem.alpha_test.hex>>16)&0xff);
#endif

	u32 usedtextures = 0;
	for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
		if (bpmem.tevorders[i / 2].getEnable(i & 1))
			usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);

	if (bpmem.genMode.numindstages > 0)
		for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);

	for (u32 i = 0; i < 8; ++i)
	{
		if (usedtextures & (1 << i))
		{
			// TODO:
			//glActiveTexture(GL_TEXTURE0 + i);

			Renderer::SetSamplerState(i & 3, i >> 2);
			FourTexUnits &tex = bpmem.tex[i >> 2];

			TCacheEntry::TCacheEntryBase* tentry = TextureCache::Load(i, 
				(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
				tex.texTlut[i&3].tlut_format,
				(tex.texMode0[i&3].min_filter & 3) && (tex.texMode0[i&3].min_filter != 8),
				(tex.texMode1[i&3].max_lod >> 4));

			if (tentry)
			{
				// 0s are probably for no manual wrapping needed.
				PixelShaderManager::SetTexDims(i, tentry->nativeW, tentry->nativeH, 0, 0);
			}
			else
			{
				ERROR_LOG(VIDEO, "Error loading texture");
			}
		}
	}

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	// finally bind
	if (false == PixelShaderCache::SetShader(false, g_nativeVertexFmt->m_components))
		return;
	if (false == VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
		return;

	const int stride = g_nativeVertexFmt->GetVertexStride();
	//if (g_nativeVertexFmt)
		g_nativeVertexFmt->SetupVertexPointers();

	g_renderer->ResumePixelPerf(false);
	g_vertex_manager->Draw(stride, false);
	g_renderer->PausePixelPerf(false);

	// run through vertex groups again to set alpha
	if (false == g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate)
	{
		if (false == PixelShaderCache::SetShader(true, g_nativeVertexFmt->m_components))
			return;

		g_vertex_manager->Draw(stride, true);
	}

	// TODO:
	//IndexGenerator::Start(TIBuffer, LIBuffer, PIBuffer);

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS) 
	{
		// save the shaders
		char strfile[255];
		sprintf(strfile, "%sps%.3d.txt", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), g_ActiveConfig.iSaveTargetId);
		std::ofstream fps;
		OpenFStream(fps, strfile, std::ios_base::out);
		fps << ps->strprog.c_str();
		sprintf(strfile, "%svs%.3d.txt", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), g_ActiveConfig.iSaveTargetId);
		std::ofstream fvs;
		OpenFStream(fvs, strfile, std::ios_base::out);
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
		Renderer::SaveRenderTarget(str, tr);
	}
#endif
	++g_Config.iSaveTargetId;
}
#endif

void VertexManager::DoState(PointerWrap& p)
{
	g_vertex_manager->vDoState(p);
}

void VertexManager::DoStateShared(PointerWrap& p)
{
	// It seems we half-assume to be flushed here
	// We update s_pCurBufferPointer yet don't worry about IndexGenerator's outdated pointers
	// and maybe other things are overlooked
	
	p.Do(LocalVBuffer);
	p.Do(TIBuffer);
	p.Do(LIBuffer);
	p.Do(PIBuffer);
	
	s_pBaseBufferPointer = &LocalVBuffer[0];
	s_pEndBufferPointer = s_pBaseBufferPointer + LocalVBuffer.size();
	p.DoPointer(s_pCurBufferPointer, s_pBaseBufferPointer);
}
