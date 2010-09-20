
// VideoCommon

#include "CommonTypes.h"

#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "Statistics.h"
#include "VideoConfig.h"
#include "PixelShaderManager.h"
#include "NativeVertexFormat.h"
#include "VertexShaderManager.h"
#include "Profiler.h"

// 
#include "TextureCache.h"
#include "VertexManager.h"
#include "Main.h"

u16* VertexManagerBase::TIBuffer;
u16* VertexManagerBase::LIBuffer;
u16* VertexManagerBase::PIBuffer;
bool VertexManagerBase::Flushed;

int VertexManagerBase::lastPrimitive;
u8* VertexManagerBase::LocalVBuffer;

// TODO: put this in .h perhaps
extern NativeVertexFormat* g_nativeVertexFmt;

NativeVertexFormat* NativeVertexFormat::Create()
{
	return g_vertex_manager->CreateNativeVertexFormat();
}

void VertexManagerBase::ResetBuffer()
{
	s_pCurBufferPointer = LocalVBuffer;
}

VertexManagerBase::VertexManagerBase()
{
	LocalVBuffer = new u8[MAXVBUFFERSIZE];
	TIBuffer = new u16[MAXIBUFFERSIZE];
	LIBuffer = new u16[MAXIBUFFERSIZE];
	PIBuffer = new u16[MAXIBUFFERSIZE];

	s_pCurBufferPointer = LocalVBuffer;
	s_pBaseBufferPointer = LocalVBuffer;

	Flushed = false;

	IndexGenerator::Start(TIBuffer, LIBuffer, PIBuffer);
}

VertexManagerBase::~VertexManagerBase()
{
	delete[] LocalVBuffer;
	delete[] TIBuffer;
	delete[] LIBuffer;
	delete[] PIBuffer;

	// most likely not needed
	ResetBuffer();
}

void VertexManagerBase::AddIndices(int _primitive, int _numVertices)
{
	switch (_primitive)
	{
	case GX_DRAW_QUADS:
		IndexGenerator::AddQuads(_numVertices);
		break;

	case GX_DRAW_TRIANGLES:
		IndexGenerator::AddList(_numVertices);
		break;

	case GX_DRAW_TRIANGLE_STRIP:
		IndexGenerator::AddStrip(_numVertices);
		break;

	case GX_DRAW_TRIANGLE_FAN:
		IndexGenerator::AddFan(_numVertices);
		break;

	case GX_DRAW_LINE_STRIP:
		IndexGenerator::AddLineStrip(_numVertices);
		break;

	case GX_DRAW_LINES:
		IndexGenerator::AddLineList(_numVertices);
		break;

	case GX_DRAW_POINTS:
		IndexGenerator::AddPoints(_numVertices);
		break;
	}
}

int VertexManager::GetRemainingSize()
{
	return VertexManagerBase::GetRemainingSize();
}

int VertexManagerBase::GetRemainingSize()
{
	return MAXVBUFFERSIZE - (int)(s_pCurBufferPointer - LocalVBuffer);
	return 0;
}

int VertexManagerBase::GetRemainingVertices(int primitive)
{
	switch (primitive)
	{
	case GX_DRAW_QUADS:
	case GX_DRAW_TRIANGLES:
	case GX_DRAW_TRIANGLE_STRIP:
	case GX_DRAW_TRIANGLE_FAN:
		return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 3;
		break;

	case GX_DRAW_LINE_STRIP:
	case GX_DRAW_LINES:
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

void VertexManager::AddVertices(int _primitive, int _numVertices)
{
	VertexManagerBase::AddVertices(_primitive, _numVertices);
}

void VertexManagerBase::AddVertices(int _primitive, int _numVertices)
{
	if (_numVertices <= 0)
		return;

	switch (_primitive)
	{
	case GX_DRAW_QUADS:
	case GX_DRAW_TRIANGLES:
	case GX_DRAW_TRIANGLE_STRIP:
	case GX_DRAW_TRIANGLE_FAN:
		if (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen() < 3 * _numVertices)
			g_vertex_manager->Flush();
		break;

	case GX_DRAW_LINE_STRIP:
	case GX_DRAW_LINES:
		if (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen() < 2 * _numVertices)
			g_vertex_manager->Flush();
		break;

	case GX_DRAW_POINTS:
		if (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen() < _numVertices)
			g_vertex_manager->Flush();
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

	lastPrimitive = _primitive;
	ADDSTAT(stats.thisFrame.numPrims, _numVertices);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	AddIndices(_primitive, _numVertices);
}

void VertexManager::Flush()
{
	VertexManagerBase::Flush();
}

void VertexManagerBase::Flush()
{
	if (LocalVBuffer == s_pCurBufferPointer)
		return;

	if (Flushed)
		return;

	Flushed = true;

	VideoFifo_CheckEFBAccess();

	DVSTARTPROFILE();

	u32 usedtextures = 0;
	for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
	{
		if (bpmem.tevorders[i/2].getEnable(i & 1))
			usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);
	}

	if (bpmem.genMode.numindstages > 0)
		for (unsigned int i = 0; i < bpmem.genMode.numtevstages + 1; ++i)
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);

	for (unsigned int i = 0; i < 8; ++i)
	{
		if (usedtextures & (1 << i))
		{
			g_renderer->SetSamplerState(i & 3, i >> 2);

			FourTexUnits &tex = bpmem.tex[i >> 2];

			TextureCacheBase::TCacheEntryBase* tentry = g_texture_cache->Load(i, 
				(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
				tex.texTlut[i&3].tlut_format,
				(tex.texMode0[i&3].min_filter & 3) && (tex.texMode0[i&3].min_filter != 8) && g_ActiveConfig.bUseNativeMips,
				(tex.texMode1[i&3].max_lod >> 4));

			if (tentry)
				PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, 0, 0);
			else
				ERROR_LOG(VIDEO, "error loading texture");
		}
	}

	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	if (false == g_pixel_shader_cache->SetShader(false))
		goto shader_fail;

	if (false == g_vertex_shader_cache->SetShader(g_nativeVertexFmt->m_components))
		goto shader_fail;

	const unsigned int stride = g_nativeVertexFmt->GetVertexStride();
	g_nativeVertexFmt->SetupVertexPointers();

	// TODO:
	g_vertex_manager->Draw(stride, false);

	if (false == g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate)
	{
		if (false == g_pixel_shader_cache->SetShader(true))
			goto shader_fail;

		// update alpha only
		g_vertex_manager->Draw(stride, true);
	}

	//IndexGenerator::Start(TIBuffer, LIBuffer, PIBuffer);

shader_fail:
	ResetBuffer();
}
