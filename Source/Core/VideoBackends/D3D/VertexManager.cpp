// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VertexManager.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/MainBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

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
	m_index_draw_offset = 0;
	m_index_buffers = new PID3D11Buffer[MAX_VBUFFER_COUNT];
	m_vertex_buffers = new PID3D11Buffer[MAX_VBUFFER_COUNT];
	for (m_current_index_buffer = 0; m_current_index_buffer < MAX_VBUFFER_COUNT; m_current_index_buffer++)
	{
		m_index_buffers[m_current_index_buffer] = nullptr;
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, nullptr, &m_index_buffers[m_current_index_buffer])),
		"Failed to create index buffer.");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_index_buffers[m_current_index_buffer], "index buffer of VertexManager");
	}
	bufdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufdesc.ByteWidth = VBUFFER_SIZE;
	for (m_current_vertex_buffer = 0; m_current_vertex_buffer < MAX_VBUFFER_COUNT; m_current_vertex_buffer++)
	{
		m_vertex_buffers[m_current_vertex_buffer] = nullptr;
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, nullptr, &m_vertex_buffers[m_current_vertex_buffer])),
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
	LocalVBuffer.resize(MAXVBUFFERSIZE);
	s_pCurBufferPointer = s_pBaseBufferPointer = &LocalVBuffer[0];
	s_pEndBufferPointer = s_pBaseBufferPointer + LocalVBuffer.size();

	LocalIBuffer.resize(MAXIBUFFERSIZE);

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

	UINT iCount = IndexGenerator::GetIndexLen();
	MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (m_index_buffer_cursor + iCount >= (IBUFFER_SIZE / sizeof(u16)))
	{
		// Wrap around
		m_current_index_buffer = (m_current_index_buffer + 1) % MAX_VBUFFER_COUNT;
		m_index_buffer_cursor = 0;
		MapType = D3D11_MAP_WRITE_DISCARD;
	}
	D3D::context->Map(m_index_buffers[m_current_index_buffer], 0, MapType, 0, &map);

	memcpy((u16*)map.pData + m_index_buffer_cursor, GetIndexBuffer(), sizeof(u16) * IndexGenerator::GetIndexLen());
	D3D::context->Unmap(m_index_buffers[m_current_index_buffer], 0);
	m_index_draw_offset = m_index_buffer_cursor;
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

	if (current_primitive_type == PRIMITIVE_TRIANGLES)
	{
		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		D3D::context->DrawIndexed(IndexGenerator::GetIndexLen(), m_index_draw_offset, 0);
		INCSTAT(stats.thisFrame.numDrawCalls);
	}
	else if (current_primitive_type == PRIMITIVE_LINES)
	{
		float lineWidth = float(bpmem.lineptwidth.linesize) / 6.f;
		float texOffset = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.lineoff];
		float vpWidth = 2.0f * xfmem.viewport.wd;
		float vpHeight = -2.0f * xfmem.viewport.ht;

		bool texOffsetEnable[8];

		for (int i = 0; i < 8; ++i)
			texOffsetEnable[i] = bpmem.texcoords[i].s.line_offset;

		if (m_lineShader.SetShader(g_nativeVertexFmt->m_components, lineWidth,
			texOffset, vpWidth, vpHeight, texOffsetEnable))
		{
			((DX11::Renderer*)g_renderer)->ApplyCullDisable(); // Disable culling for lines and points
			D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
			D3D::context->DrawIndexed(IndexGenerator::GetIndexLen(), m_index_draw_offset, 0);
			INCSTAT(stats.thisFrame.numDrawCalls);

			D3D::context->GSSetShader(nullptr, nullptr, 0);
			((DX11::Renderer*)g_renderer)->RestoreCull();
		}
	}
	else //if (current_primitive_type == PRIMITIVE_POINTS)
	{
		float pointSize = float(bpmem.lineptwidth.pointsize) / 6.f;
		float texOffset = LINE_PT_TEX_OFFSETS[bpmem.lineptwidth.pointoff];
		float vpWidth = 2.0f * xfmem.viewport.wd;
		float vpHeight = -2.0f * xfmem.viewport.ht;

		bool texOffsetEnable[8];

		for (int i = 0; i < 8; ++i)
			texOffsetEnable[i] = bpmem.texcoords[i].s.point_offset;

		if (m_pointShader.SetShader(g_nativeVertexFmt->m_components, pointSize,
			texOffset, vpWidth, vpHeight, texOffsetEnable))
		{
			((DX11::Renderer*)g_renderer)->ApplyCullDisable(); // Disable culling for lines and points
			D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			D3D::context->DrawIndexed(IndexGenerator::GetIndexLen(), m_index_draw_offset, 0);
			INCSTAT(stats.thisFrame.numDrawCalls);

			D3D::context->GSSetShader(nullptr, nullptr, 0);
			((DX11::Renderer*)g_renderer)->RestoreCull();
		}
	}
}

void VertexManager::vFlush(bool useDstAlpha)
{
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

	Draw(stride);

	g_renderer->RestoreState();
}

void VertexManager::ResetBuffer(u32 stride)
{
	s_pCurBufferPointer = s_pBaseBufferPointer;
	IndexGenerator::Start(GetIndexBuffer());
}

}  // namespace
