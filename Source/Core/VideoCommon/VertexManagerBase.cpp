#include "Common/CommonTypes.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/MainBase.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

VertexManager *g_vertex_manager;

u8 *VertexManager::s_pCurBufferPointer;
u8 *VertexManager::s_pBaseBufferPointer;
u8 *VertexManager::s_pEndBufferPointer;

PrimitiveType VertexManager::current_primitive_type;

bool VertexManager::IsFlushed;

static const PrimitiveType primitive_from_gx[8] = {
	PRIMITIVE_TRIANGLES, // GX_DRAW_QUADS
	PRIMITIVE_TRIANGLES, // GX_DRAW_QUADS_2
	PRIMITIVE_TRIANGLES, // GX_DRAW_TRIANGLES
	PRIMITIVE_TRIANGLES, // GX_DRAW_TRIANGLE_STRIP
	PRIMITIVE_TRIANGLES, // GX_DRAW_TRIANGLE_FAN
	PRIMITIVE_LINES,     // GX_DRAW_LINES
	PRIMITIVE_LINES,     // GX_DRAW_LINE_STRIP
	PRIMITIVE_POINTS,    // GX_DRAW_POINTS
};

VertexManager::VertexManager()
{
	IsFlushed = true;
}

VertexManager::~VertexManager()
{
}

u32 VertexManager::GetRemainingSize()
{
	return (u32)(s_pEndBufferPointer - s_pCurBufferPointer);
}

void VertexManager::PrepareForAdditionalData(int primitive, u32 count, u32 stride)
{
	// The SSE vertex loader can write up to 4 bytes past the end
	u32 const needed_vertex_bytes = count * stride + 4;

	// We can't merge different kinds of primitives, so we have to flush here
	if (current_primitive_type != primitive_from_gx[primitive])
		Flush();
	current_primitive_type = primitive_from_gx[primitive];

	// Check for size in buffer, if the buffer gets full, call Flush()
	if ( !IsFlushed && ( count > IndexGenerator::GetRemainingIndices() ||
	     count > GetRemainingIndices(primitive) || needed_vertex_bytes > GetRemainingSize() ) )
	{
		Flush();

		if (count > IndexGenerator::GetRemainingIndices())
			ERROR_LOG(VIDEO, "Too little remaining index values. Use 32-bit or reset them on flush.");
		if (count > GetRemainingIndices(primitive))
			ERROR_LOG(VIDEO, "VertexManager: Buffer not large enough for all indices! "
				"Increase MAXIBUFFERSIZE or we need primitive breaking after all.");
		if (needed_vertex_bytes > GetRemainingSize())
			ERROR_LOG(VIDEO, "VertexManager: Buffer not large enough for all vertices! "
				"Increase MAXVBUFFERSIZE or we need primitive breaking after all.");
	}

	// need to alloc new buffer
	if (IsFlushed)
	{
		g_vertex_manager->ResetBuffer(stride);
		IsFlushed = false;
	}
}

u32 VertexManager::GetRemainingIndices(int primitive)
{
	u32 index_len = MAXIBUFFERSIZE - IndexGenerator::GetIndexLen();

	if (g_Config.backend_info.bSupportsPrimitiveRestart)
	{
		switch (primitive)
		{
		case GX_DRAW_QUADS:
		case GX_DRAW_QUADS_2:
			return index_len / 5 * 4;
		case GX_DRAW_TRIANGLES:
			return index_len / 4 * 3;
		case GX_DRAW_TRIANGLE_STRIP:
			return index_len / 1 - 1;
		case GX_DRAW_TRIANGLE_FAN:
			return index_len / 6 * 4 + 1;

		case GX_DRAW_LINES:
			return index_len;
		case GX_DRAW_LINE_STRIP:
			return index_len / 2 + 1;

		case GX_DRAW_POINTS:
			return index_len;

		default:
			return 0;
		}
	}
	else
	{
		switch (primitive)
		{
		case GX_DRAW_QUADS:
		case GX_DRAW_QUADS_2:
			return index_len / 6 * 4;
		case GX_DRAW_TRIANGLES:
			return index_len;
		case GX_DRAW_TRIANGLE_STRIP:
			return index_len / 3 + 2;
		case GX_DRAW_TRIANGLE_FAN:
			return index_len / 3 + 2;

		case GX_DRAW_LINES:
			return index_len;
		case GX_DRAW_LINE_STRIP:
			return index_len / 2 + 1;

		case GX_DRAW_POINTS:
			return index_len;

		default:
			return 0;
		}
	}
}

void VertexManager::Flush()
{
	if (IsFlushed)
		return;

	// loading a state will invalidate BP, so check for it
	g_video_backend->CheckInvalidState();

	VideoFifo_CheckEFBAccess();

#if defined(_DEBUG) || defined(DEBUGFAST)
	PRIM_LOG("frame%d:\n texgen=%d, numchan=%d, dualtex=%d, ztex=%d, cole=%d, alpe=%d, ze=%d", g_ActiveConfig.iSaveTargetId, xfmem.numTexGen.numTexGens,
		xfmem.numChan.numColorChans, xfmem.dualTexTrans.enabled, bpmem.ztex2.op,
		(int)bpmem.blendmode.colorupdate, (int)bpmem.blendmode.alphaupdate, (int)bpmem.zmode.updateenable);

	for (unsigned int i = 0; i < xfmem.numChan.numColorChans; ++i)
	{
		LitChannel* ch = &xfmem.color[i];
		PRIM_LOG("colchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
		ch = &xfmem.alpha[i];
		PRIM_LOG("alpchan%d: matsrc=%d, light=0x%x, ambsrc=%d, diffunc=%d, attfunc=%d", i, ch->matsource, ch->GetFullLightMask(), ch->ambsource, ch->diffusefunc, ch->attnfunc);
	}

	for (unsigned int i = 0; i < xfmem.numTexGen.numTexGens; ++i)
	{
		TexMtxInfo tinfo = xfmem.texMtxInfo[i];
		if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP) tinfo.hex &= 0x7ff;
		if (tinfo.texgentype != XF_TEXGEN_REGULAR) tinfo.projection = 0;

		PRIM_LOG("txgen%d: proj=%d, input=%d, gentype=%d, srcrow=%d, embsrc=%d, emblght=%d, postmtx=%d, postnorm=%d",
			i, tinfo.projection, tinfo.inputform, tinfo.texgentype, tinfo.sourcerow, tinfo.embosssourceshift, tinfo.embosslightshift,
			xfmem.postMtxInfo[i].index, xfmem.postMtxInfo[i].normalize);
	}

	PRIM_LOG("pixel: tev=%d, ind=%d, texgen=%d, dstalpha=%d, alphatest=0x%x", (int)bpmem.genMode.numtevstages+1, (int)bpmem.genMode.numindstages,
		(int)bpmem.genMode.numtexgens, (u32)bpmem.dstalpha.enable, (bpmem.alpha_test.hex>>16)&0xff);
#endif

	BitSet32 usedtextures;
	for (u32 i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
		if (bpmem.tevorders[i / 2].getEnable(i & 1))
			usedtextures[bpmem.tevorders[i/2].getTexMap(i & 1)] = true;

	if (bpmem.genMode.numindstages > 0)
		for (unsigned int i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				usedtextures[bpmem.tevindref.getTexMap(bpmem.tevind[i].bt)] = true;

	for (unsigned int i : usedtextures)
	{
		g_renderer->SetSamplerState(i & 3, i >> 2);
		const FourTexUnits &tex = bpmem.tex[i >> 2];
		const TextureCache::TCacheEntryBase* tentry = TextureCache::Load(i,
			(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
			tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
			tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9,
			tex.texTlut[i&3].tlut_format,
			((tex.texMode0[i&3].min_filter & 3) != 0),
			(tex.texMode1[i&3].max_lod + 0xf) / 0x10,
			(tex.texImage1[i&3].image_type != 0));

		if (tentry)
		{
			// 0s are probably for no manual wrapping needed.
			PixelShaderManager::SetTexDims(i, tentry->native_width, tentry->native_height, 0, 0);
		}
		else
			ERROR_LOG(VIDEO, "error loading texture");
	}

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass &&
	                   bpmem.dstalpha.enable &&
	                   bpmem.blendmode.alphaupdate &&
	                   bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;

	if (PerfQueryBase::ShouldEmulate())
		g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
	g_vertex_manager->vFlush(useDstAlpha);
	if (PerfQueryBase::ShouldEmulate())
		g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

	if (xfmem.numTexGen.numTexGens != bpmem.genMode.numtexgens)
		ERROR_LOG(VIDEO, "xf.numtexgens (%d) does not match bp.numtexgens (%d). Error in command stream.", xfmem.numTexGen.numTexGens, bpmem.genMode.numtexgens.Value());

	IsFlushed = true;
}

void VertexManager::DoState(PointerWrap& p)
{
	g_vertex_manager->vDoState(p);
}
