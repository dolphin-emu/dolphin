
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
#include "VideoConfig.h"

VertexManager *g_vertex_manager;

u8 *VertexManager::s_pCurBufferPointer;
u8 *VertexManager::s_pBaseBufferPointer;

u8 *VertexManager::LocalVBuffer;
u16 *VertexManager::TIBuffer;
u16 *VertexManager::LIBuffer;
u16 *VertexManager::PIBuffer;

bool VertexManager::Flushed;

VertexManager::VertexManager()
{
	Flushed = false;

	LocalVBuffer = new u8[MAXVBUFFERSIZE];
	s_pCurBufferPointer = s_pBaseBufferPointer = LocalVBuffer;

	TIBuffer = new u16[MAXIBUFFERSIZE];
	LIBuffer = new u16[MAXIBUFFERSIZE];
	PIBuffer = new u16[MAXIBUFFERSIZE];

	IndexGenerator::Start(TIBuffer, LIBuffer, PIBuffer);
}

void VertexManager::ResetBuffer()
{
	s_pCurBufferPointer = LocalVBuffer;
}

VertexManager::~VertexManager()
{
	delete[] LocalVBuffer;

	delete[] TIBuffer;
	delete[] LIBuffer;
	delete[] PIBuffer;

	// TODO: necessary??
	ResetBuffer();
}

void VertexManager::AddIndices(int primitive, int numVertices)
{
	//switch (primitive)
	//{
	//case GX_DRAW_QUADS:          IndexGenerator::AddQuads(numVertices);		break;
	//case GX_DRAW_TRIANGLES:      IndexGenerator::AddList(numVertices);		break;
	//case GX_DRAW_TRIANGLE_STRIP: IndexGenerator::AddStrip(numVertices);		break;
	//case GX_DRAW_TRIANGLE_FAN:   IndexGenerator::AddFan(numVertices);		break;
	//case GX_DRAW_LINES:		   IndexGenerator::AddLineList(numVertices);	break;
	//case GX_DRAW_LINE_STRIP:     IndexGenerator::AddLineStrip(numVertices);	break;
	//case GX_DRAW_POINTS:         IndexGenerator::AddPoints(numVertices);	break;
	//}

	static void (*const primitive_table[])(int) =
	{
		IndexGenerator::AddQuads,
		NULL,
		IndexGenerator::AddList,
		IndexGenerator::AddStrip,
		IndexGenerator::AddFan,
		IndexGenerator::AddLineList,
		IndexGenerator::AddLineStrip,
		IndexGenerator::AddPoints,
	};

	primitive_table[primitive](numVertices);
}

int VertexManager::GetRemainingSize()
{
	return MAXVBUFFERSIZE - (int)(s_pCurBufferPointer - LocalVBuffer);
}

int VertexManager::GetRemainingVertices(int primitive)
{
	switch (primitive)
	{
	case GX_DRAW_QUADS:
	case GX_DRAW_TRIANGLES:
	case GX_DRAW_TRIANGLE_STRIP:
	case GX_DRAW_TRIANGLE_FAN:
		return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 3;
		break;

	case GX_DRAW_LINES:
	case GX_DRAW_LINE_STRIP:
		return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen()) / 2;
		break;

	case GX_DRAW_POINTS:
		return (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen());
		break;

	default:
		return 0;
		break;
	}
}

void VertexManager::AddVertices(int primitive, int numVertices)
{
	if (numVertices <= 0)
		return;

	switch (primitive)
	{
	case GX_DRAW_QUADS:
	case GX_DRAW_TRIANGLES:
	case GX_DRAW_TRIANGLE_STRIP:
	case GX_DRAW_TRIANGLE_FAN:
		if (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen() < 3 * numVertices)
			Flush();
		break;

	case GX_DRAW_LINES:
	case GX_DRAW_LINE_STRIP:
		if (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen() < 2 * numVertices)
			Flush();
		break;

	case GX_DRAW_POINTS:
		if (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen() < numVertices)
			Flush();
		break;

	default:
		return;
		break;
	}

	if (Flushed)
	{
		IndexGenerator::Start(TIBuffer, LIBuffer, PIBuffer);
		Flushed = false;
	}

	ADDSTAT(stats.thisFrame.numPrims, numVertices);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	AddIndices(primitive, numVertices);
}

void VertexManager::Flush()
{
	// loading a state will invalidate BP, so check for it
	g_video_backend->CheckInvalidState();
	
	g_vertex_manager->vFlush();
}

// TODO: need to merge more stuff into VideoCommon to use this
#if (0)
void VertexManager::Flush()
{
	if (LocalVBuffer == s_pCurBufferPointer || Flushed)
		return;

	Flushed = true;

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
				ERROR_LOG(VIDEO, "error loading texture");
		}
	}

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	// finally bind
	if (false == PixelShaderCache::SetShader(false, g_nativeVertexFmt->m_components))
		goto shader_fail;
	if (false == VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
		goto shader_fail;

	const int stride = g_nativeVertexFmt->GetVertexStride();
	//if (g_nativeVertexFmt)
		g_nativeVertexFmt->SetupVertexPointers();

	g_vertex_manager->Draw(stride, false);

	// run through vertex groups again to set alpha
	if (false == g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate)
	{
		if (false == PixelShaderCache::SetShader(true, g_nativeVertexFmt->m_components))
			goto shader_fail;

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
		Renderer::SaveRenderTarget(str, tr);
	}
#endif
	++g_Config.iSaveTargetId;

shader_fail:
	ResetBuffer();
}
#endif

void VertexManager::DoState(PointerWrap& p)
{
	g_vertex_manager->vDoState(p);
}

void VertexManager::DoStateShared(PointerWrap& p)
{
	p.DoPointer(s_pCurBufferPointer, LocalVBuffer);
	p.DoArray(LocalVBuffer, MAXVBUFFERSIZE);
	p.DoArray(TIBuffer, MAXIBUFFERSIZE);
	p.DoArray(LIBuffer, MAXIBUFFERSIZE);
	p.DoArray(PIBuffer, MAXIBUFFERSIZE);

	if (p.GetMode() == PointerWrap::MODE_READ)
		Flushed = false;
}
