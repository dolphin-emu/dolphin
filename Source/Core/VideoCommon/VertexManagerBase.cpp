// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/CommonTypes.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

std::unique_ptr<VertexManagerBase> g_vertex_manager;

u8* VertexManagerBase::s_pCurBufferPointer;
u8* VertexManagerBase::s_pBaseBufferPointer;
u8* VertexManagerBase::s_pEndBufferPointer;

PrimitiveType VertexManagerBase::current_primitive_type;

Slope VertexManagerBase::s_zslope;

bool VertexManagerBase::s_is_flushed;
bool VertexManagerBase::s_cull_all;

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

VertexManagerBase::VertexManagerBase()
{
	s_is_flushed = true;
	s_cull_all = false;
}

VertexManagerBase::~VertexManagerBase()
{
}

u32 VertexManagerBase::GetRemainingSize()
{
	return (u32)(s_pEndBufferPointer - s_pCurBufferPointer);
}

DataReader VertexManagerBase::PrepareForAdditionalData(int primitive, u32 count, u32 stride, bool cullall)
{
	// The SSE vertex loader can write up to 4 bytes past the end
	u32 const needed_vertex_bytes = count * stride + 4;

	// We can't merge different kinds of primitives, so we have to flush here
	if (current_primitive_type != primitive_from_gx[primitive])
		Flush();
	current_primitive_type = primitive_from_gx[primitive];

	// Check for size in buffer, if the buffer gets full, call Flush()
	if (!s_is_flushed && ( count > IndexGenerator::GetRemainingIndices() ||
	     count > GetRemainingIndices(primitive) || needed_vertex_bytes > GetRemainingSize()))
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

	s_cull_all = cullall;

	// need to alloc new buffer
	if (s_is_flushed)
	{
		g_vertex_manager->ResetBuffer(stride);
		s_is_flushed = false;
	}

	return DataReader(s_pCurBufferPointer, s_pEndBufferPointer);
}

void VertexManagerBase::FlushData(u32 count, u32 stride)
{
	s_pCurBufferPointer += count * stride;
}

u32 VertexManagerBase::GetRemainingIndices(int primitive)
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

void VertexManagerBase::Flush()
{
	if (s_is_flushed)
		return;

	// loading a state will invalidate BP, so check for it
	g_video_backend->CheckInvalidState();

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

	// If the primitave is marked CullAll. All we need to do is update the vertex constants and calculate the zfreeze refrence slope
	if (!s_cull_all)
	{
		BitSet32 usedtextures;
		for (u32 i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
			if (bpmem.tevorders[i / 2].getEnable(i & 1))
				usedtextures[bpmem.tevorders[i/2].getTexMap(i & 1)] = true;

		if (bpmem.genMode.numindstages > 0)
			for (unsigned int i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
				if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
					usedtextures[bpmem.tevindref.getTexMap(bpmem.tevind[i].bt)] = true;

		TextureCacheBase::UnbindTextures();
		for (unsigned int i : usedtextures)
		{
			const TextureCacheBase::TCacheEntryBase* tentry = TextureCacheBase::Load(i);

			if (tentry)
			{
				g_renderer->SetSamplerState(i & 3, i >> 2, tentry->is_custom_tex);
				PixelShaderManager::SetTexDims(i, tentry->native_width, tentry->native_height);
			}
			else
			{
				ERROR_LOG(VIDEO, "error loading texture");
			}
		}
		TextureCacheBase::BindTextures();
	}

	// set global vertex constants
	VertexShaderManager::SetConstants();

	// Calculate ZSlope for zfreeze
	if (!bpmem.genMode.zfreeze)
	{
		// Must be done after VertexShaderManager::SetConstants()
		CalculateZSlope(VertexLoaderManager::GetCurrentVertexFormat());
	}
	else if (s_zslope.dirty && !s_cull_all) // or apply any dirty ZSlopes
	{
		PixelShaderManager::SetZSlope(s_zslope.dfdx, s_zslope.dfdy, s_zslope.f0);
		s_zslope.dirty = false;
	}

	if (!s_cull_all)
	{
		// set the rest of the global constants
		GeometryShaderManager::SetConstants();
		PixelShaderManager::SetConstants();

		bool useDstAlpha = bpmem.dstalpha.enable &&
		                   bpmem.blendmode.alphaupdate &&
		                   bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;

		if (PerfQueryBase::ShouldEmulate())
			g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
		g_vertex_manager->vFlush(useDstAlpha);
		if (PerfQueryBase::ShouldEmulate())
			g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
	}

	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

	if (xfmem.numTexGen.numTexGens != bpmem.genMode.numtexgens)
		ERROR_LOG(VIDEO, "xf.numtexgens (%d) does not match bp.numtexgens (%d). Error in command stream.", xfmem.numTexGen.numTexGens, bpmem.genMode.numtexgens.Value());

	s_is_flushed = true;
	s_cull_all = false;
}

void VertexManagerBase::DoState(PointerWrap& p)
{
	p.Do(s_zslope);
	g_vertex_manager->vDoState(p);
}

void VertexManagerBase::CalculateZSlope(NativeVertexFormat* format)
{
	float out[12];
	float viewOffset[2] = { xfmem.viewport.xOrig - bpmem.scissorOffset.x * 2,
	                        xfmem.viewport.yOrig - bpmem.scissorOffset.y * 2};

	if (current_primitive_type != PRIMITIVE_TRIANGLES)
		return;

	// Global matrix ID.
	u32 mtxIdx = g_main_cp_state.matrix_index_a.PosNormalMtxIdx;
	const PortableVertexDeclaration vert_decl = format->GetVertexDeclaration();

	// Make sure the buffer contains at least 3 vertices.
	if ((s_pCurBufferPointer - s_pBaseBufferPointer) < (vert_decl.stride * 3))
		return;

	// Lookup vertices of the last rendered triangle and software-transform them
	// This allows us to determine the depth slope, which will be used if z-freeze
	// is enabled in the following flush.
	for (unsigned int i = 0; i < 3; ++i)
	{
		// If this vertex format has per-vertex position matrix IDs, look it up.
		if (vert_decl.posmtx.enable)
			mtxIdx = VertexLoaderManager::position_matrix_index[2 - i];

		if (vert_decl.position.components == 2)
			VertexLoaderManager::position_cache[2 - i][2] = 0;

		VertexShaderManager::TransformToClipSpace(&VertexLoaderManager::position_cache[2 - i][0], &out[i * 4], mtxIdx);

		// Transform to Screenspace
		float inv_w = 1.0f / out[3 + i * 4];

		out[0 + i * 4] = out[0 + i * 4] * inv_w * xfmem.viewport.wd + viewOffset[0];
		out[1 + i * 4] = out[1 + i * 4] * inv_w * xfmem.viewport.ht + viewOffset[1];
		out[2 + i * 4] = out[2 + i * 4] * inv_w * xfmem.viewport.zRange + xfmem.viewport.farZ;
	}

	float dx31 = out[8] - out[0];
	float dx12 = out[0] - out[4];
	float dy12 = out[1] - out[5];
	float dy31 = out[9] - out[1];

	float DF31 = out[10] - out[2];
	float DF21 = out[6] - out[2];
	float a = DF31 * -dy12 - DF21 * dy31;
	float b = dx31 * DF21 + dx12 * DF31;
	float c = -dx12 * dy31 - dx31 * -dy12;

	// Sometimes we process de-generate triangles. Stop any divide by zeros
	if (c == 0)
		return;

	s_zslope.dfdx = -a / c;
	s_zslope.dfdy = -b / c;
	s_zslope.f0 = out[2] - (out[0] * s_zslope.dfdx + out[1] * s_zslope.dfdy);
	s_zslope.dirty = true;
}
