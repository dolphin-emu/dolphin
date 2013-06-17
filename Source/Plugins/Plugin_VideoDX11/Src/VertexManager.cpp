// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "D3DBase.h"
#include "PixelShaderCache.h"
#include "VertexManager.h"
#include "VertexShaderCache.h"

#include "BPMemory.h"
#include "Debugger.h"
#include "IndexGenerator.h"
#include "MainBase.h"
#include "PixelShaderManager.h"
#include "RenderBase.h"
#include "Render.h"
#include "Statistics.h"
#include "TextureCacheBase.h"
#include "VertexShaderManager.h"
#include "VideoConfig.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace DX11
{

// TODO: Find sensible values for these two
const UINT IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 8;
const UINT VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE;
const UINT MAX_VBUFFER_COUNT = 2;

void VertexManager::CreateDeviceObjects()
{
	D3D11_BUFFER_DESC bufdesc = CD3D11_BUFFER_DESC(IBUFFER_SIZE,
		D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	m_vertex_draw_offset = 0;
	m_triangle_draw_index = 0;
	m_line_draw_index = 0;
	m_point_draw_index = 0;
	m_index_buffers = new PID3D11Buffer[MAX_VBUFFER_COUNT];
	m_vertex_buffers = new PID3D11Buffer[MAX_VBUFFER_COUNT];	
	for (m_current_index_buffer = 0; m_current_index_buffer < MAX_VBUFFER_COUNT; m_current_index_buffer++)
	{
		m_index_buffers[m_current_index_buffer] = NULL;
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, NULL, &m_index_buffers[m_current_index_buffer])),
		"Failed to create index buffer.");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_index_buffers[m_current_index_buffer], "index buffer of VertexManager");
	}
	bufdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufdesc.ByteWidth = VBUFFER_SIZE;
	for (m_current_vertex_buffer = 0; m_current_vertex_buffer < MAX_VBUFFER_COUNT; m_current_vertex_buffer++)
	{
		m_vertex_buffers[m_current_vertex_buffer] = NULL;
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, NULL, &m_vertex_buffers[m_current_vertex_buffer])),
		"Failed to create vertex buffer.");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_vertex_buffers[m_current_vertex_buffer], "Vertex buffer of VertexManager");
	}
	m_current_vertex_buffer = 0;
	m_current_index_buffer = 0;
	m_index_buffer_cursor = IBUFFER_SIZE;
	m_vertex_buffer_cursor = VBUFFER_SIZE;
	m_lineShader.Init();
	m_pointShader.Init();
}

void VertexManager::DestroyDeviceObjects()
{
	m_pointShader.Shutdown();
	m_lineShader.Shutdown();
	for (m_current_vertex_buffer = 0; m_current_vertex_buffer < MAX_VBUFFER_COUNT; m_current_vertex_buffer++)
	{
		SAFE_RELEASE(m_vertex_buffers[m_current_vertex_buffer]);
		SAFE_RELEASE(m_index_buffers[m_current_vertex_buffer]);
	}
	
}

VertexManager::VertexManager()
{
	CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
	DestroyDeviceObjects();
}

void VertexManager::PrepareDrawBuffers()
{
	D3D11_MAPPED_SUBRESOURCE map;

	UINT vSize = UINT(s_pCurBufferPointer - s_pBaseBufferPointer);
	D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (m_vertex_buffer_cursor + vSize >= VBUFFER_SIZE)
	{
		// Wrap around
		m_current_vertex_buffer = (m_current_vertex_buffer + 1) % MAX_VBUFFER_COUNT;		
		m_vertex_buffer_cursor = 0;
		MapType = D3D11_MAP_WRITE_DISCARD;
	}

	D3D::context->Map(m_vertex_buffers[m_current_vertex_buffer], 0, MapType, 0, &map);

	memcpy((u8*)map.pData + m_vertex_buffer_cursor, s_pBaseBufferPointer, vSize);
	D3D::context->Unmap(m_vertex_buffers[m_current_vertex_buffer], 0);
	m_vertex_draw_offset = m_vertex_buffer_cursor;
	m_vertex_buffer_cursor += vSize;

	UINT iCount = IndexGenerator::GetTriangleindexLen() +
		IndexGenerator::GetLineindexLen() + IndexGenerator::GetPointindexLen();
	MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (m_index_buffer_cursor + iCount >= (IBUFFER_SIZE / sizeof(u16)))
	{
		// Wrap around
		m_current_index_buffer = (m_current_index_buffer + 1) % MAX_VBUFFER_COUNT;		
		m_index_buffer_cursor = 0;
		MapType = D3D11_MAP_WRITE_DISCARD;
	}
	D3D::context->Map(m_index_buffers[m_current_index_buffer], 0, MapType, 0, &map);

	m_triangle_draw_index = m_index_buffer_cursor;
	m_line_draw_index = m_triangle_draw_index + IndexGenerator::GetTriangleindexLen();
	m_point_draw_index = m_line_draw_index + IndexGenerator::GetLineindexLen();
	memcpy((u16*)map.pData + m_triangle_draw_index, GetTriangleIndexBuffer(), sizeof(u16) * IndexGenerator::GetTriangleindexLen());
	memcpy((u16*)map.pData + m_line_draw_index, GetLineIndexBuffer(), sizeof(u16) * IndexGenerator::GetLineindexLen());
	memcpy((u16*)map.pData + m_point_draw_index, GetPointIndexBuffer(), sizeof(u16) * IndexGenerator::GetPointindexLen());
	D3D::context->Unmap(m_index_buffers[m_current_index_buffer], 0);
	m_index_buffer_cursor += iCount;
	
	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vSize);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, iCount*sizeof(u16));
}

static const float LINE_PT_TEX_OFFSETS[8] = {
	0.f, 0.0625f, 0.125f, 0.25f, 0.5f, 1.f, 1.f, 1.f
};

void VertexManager::Draw(UINT stride)
{
	D3D::context->IASetVertexBuffers(0, 1, &m_vertex_buffers[m_current_vertex_buffer], &stride, &m_vertex_draw_offset);
	D3D::context->IASetIndexBuffer(m_index_buffers[m_current_index_buffer], DXGI_FORMAT_R16_UINT, 0);
	
	if (IndexGenerator::GetNumTriangles() > 0)
	{
		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		D3D::context->DrawIndexed(IndexGenerator::GetTriangleindexLen(), m_triangle_draw_index, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	// Disable culling for lines and points
	if (IndexGenerator::GetNumLines() > 0 || IndexGenerator::GetNumPoints() > 0)
		((DX11::Renderer*)g_renderer)->ApplyCullDisable();
	if (IndexGenerator::GetNumLines() > 0)
	{
		float lineWidth = float(bpmem.lineptwidth.linesize) / 6.f;
		float texOffset = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.lineoff];
		float vpWidth = 2.0f * xfregs.viewport.wd;
		float vpHeight = -2.0f * xfregs.viewport.ht;

		bool texOffsetEnable[8];

		for (int i = 0; i < 8; ++i)
			texOffsetEnable[i] = bpmem.texcoords[i].s.line_offset;

		if (m_lineShader.SetShader(g_nativeVertexFmt->m_components, lineWidth,
			texOffset, vpWidth, vpHeight, texOffsetEnable))
		{
			D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
			D3D::context->DrawIndexed(IndexGenerator::GetLineindexLen(), m_line_draw_index, 0);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);

			D3D::context->GSSetShader(NULL, NULL, 0);
		}
	}
	if (IndexGenerator::GetNumPoints() > 0)
	{
		float pointSize = float(bpmem.lineptwidth.pointsize) / 6.f;
		float texOffset = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.pointoff];
		float vpWidth = 2.0f * xfregs.viewport.wd;
		float vpHeight = -2.0f * xfregs.viewport.ht;

		bool texOffsetEnable[8];

		for (int i = 0; i < 8; ++i)
			texOffsetEnable[i] = bpmem.texcoords[i].s.point_offset;

		if (m_pointShader.SetShader(g_nativeVertexFmt->m_components, pointSize,
			texOffset, vpWidth, vpHeight, texOffsetEnable))
		{
			D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			D3D::context->DrawIndexed(IndexGenerator::GetPointindexLen(), m_point_draw_index, 0);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);

			D3D::context->GSSetShader(NULL, NULL, 0);
		}
	}
	if (IndexGenerator::GetNumLines() > 0 || IndexGenerator::GetNumPoints() > 0)
		((DX11::Renderer*)g_renderer)->RestoreCull();
}

void VertexManager::vFlush()
{
	u32 usedtextures = 0;
	for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
		if (bpmem.tevorders[i / 2].getEnable(i & 1))
			usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);

	if (bpmem.genMode.numindstages > 0)
		for (unsigned int i = 0; i < bpmem.genMode.numtevstages + 1; ++i)
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);

	for (unsigned int i = 0; i < 8; i++)
	{
		if (usedtextures & (1 << i))
		{
			g_renderer->SetSamplerState(i & 3, i >> 2);
			const FourTexUnits &tex = bpmem.tex[i >> 2];
			const TextureCache::TCacheEntryBase* tentry = TextureCache::Load(i, 
				(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
				tex.texTlut[i&3].tlut_format,
				(tex.texMode0[i&3].min_filter & 3),
				(tex.texMode1[i&3].max_lod + 0xf) / 0x10,
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
	PixelShaderManager::SetConstants(g_nativeVertexFmt->m_components);

	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate &&
		bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;

	if (!PixelShaderCache::SetShader(
		useDstAlpha ? DSTALPHA_DUAL_SOURCE_BLEND : DSTALPHA_NONE,
		g_nativeVertexFmt->m_components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		return;
	}
	if (!VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		return;
	}
	PrepareDrawBuffers();
	unsigned int stride = g_nativeVertexFmt->GetVertexStride();
	g_nativeVertexFmt->SetupVertexPointers();
	g_renderer->ApplyState(useDstAlpha);

	g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
	Draw(stride);
	g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

	g_renderer->RestoreState();
}

}  // namespace
