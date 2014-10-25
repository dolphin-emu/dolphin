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
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{

// TODO: Find sensible values for these two
const u32 MAX_IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 8;
const u32 MAX_VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE;
const u32 MAX_BUFFER_SIZE = MAX_IBUFFER_SIZE + MAX_VBUFFER_SIZE;

void VertexManager::CreateDeviceObjects()
{
	D3D11_BUFFER_DESC bufdesc = CD3D11_BUFFER_DESC(MAX_BUFFER_SIZE,
		D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	m_vertexDrawOffset = 0;
	m_indexDrawOffset = 0;
	
	for (int i = 0; i < MAX_BUFFER_COUNT; i++)
	{
		m_buffers[i] = nullptr;
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, nullptr, &m_buffers[i])), "Failed to create buffer.");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)m_buffers[i], "Buffer of VertexManager");
	}

	m_currentBuffer = 0;
	m_bufferCursor = MAX_BUFFER_SIZE;

	m_lineShader.Init();
	m_pointShader.Init();
}

void VertexManager::DestroyDeviceObjects()
{
	m_pointShader.Shutdown();
	m_lineShader.Shutdown();

	for (int i = 0; i < MAX_BUFFER_COUNT; i++)
	{
		SAFE_RELEASE(m_buffers[i]);
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

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	D3D11_MAPPED_SUBRESOURCE map;

	u32 vertexBufferSize = u32(s_pCurBufferPointer - s_pBaseBufferPointer);
	u32 indexBufferSize = IndexGenerator::GetIndexLen() * sizeof(u16);
	u32 totalBufferSize = vertexBufferSize + indexBufferSize;

	u32 cursor = m_bufferCursor;
	u32 padding = m_bufferCursor % stride;
	if (padding)
	{
		cursor += stride - padding;
	}

	D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (cursor + totalBufferSize >= MAX_BUFFER_SIZE)
	{
		// Wrap around
		m_currentBuffer = (m_currentBuffer + 1) % MAX_BUFFER_COUNT;
		cursor = 0;
		MapType = D3D11_MAP_WRITE_DISCARD;
	}

	m_vertexDrawOffset = cursor;
	m_indexDrawOffset = cursor + vertexBufferSize;

	D3D::context->Map(m_buffers[m_currentBuffer], 0, MapType, 0, &map);
	u8* mappedData = reinterpret_cast<u8*>(map.pData);
	memcpy(mappedData + m_vertexDrawOffset, s_pBaseBufferPointer, vertexBufferSize);
	memcpy(mappedData + m_indexDrawOffset, GetIndexBuffer(), indexBufferSize);
	D3D::context->Unmap(m_buffers[m_currentBuffer], 0);

	m_bufferCursor = cursor + totalBufferSize;

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vertexBufferSize);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, indexBufferSize);
}

static const float LINE_PT_TEX_OFFSETS[8] = {
	0.f, 0.0625f, 0.125f, 0.25f, 0.5f, 1.f, 1.f, 1.f
};

void VertexManager::Draw(u32 stride)
{
	u32 components = VertexLoaderManager::GetCurrentVertexFormat()->m_components;
	u32 indices = IndexGenerator::GetIndexLen();

	u32 zero = 0;
	D3D::context->IASetVertexBuffers(0, 1, &m_buffers[m_currentBuffer], &stride, &zero);
	D3D::context->IASetIndexBuffer(m_buffers[m_currentBuffer], DXGI_FORMAT_R16_UINT, 0);

	u32 baseVertex = m_vertexDrawOffset / stride;
	u32 startIndex = m_indexDrawOffset / sizeof(u16);

	if (current_primitive_type == PRIMITIVE_TRIANGLES)
	{
		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		D3D::context->DrawIndexed(indices, startIndex, baseVertex);
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

		if (m_lineShader.SetShader(components, lineWidth,
			texOffset, vpWidth, vpHeight, texOffsetEnable))
		{
			((DX11::Renderer*)g_renderer)->ApplyCullDisable(); // Disable culling for lines and points
			D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
			D3D::context->DrawIndexed(indices, startIndex, baseVertex);
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

		if (m_pointShader.SetShader(components, pointSize,
			texOffset, vpWidth, vpHeight, texOffsetEnable))
		{
			((DX11::Renderer*)g_renderer)->ApplyCullDisable(); // Disable culling for lines and points
			D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			D3D::context->DrawIndexed(indices, startIndex, baseVertex);
			INCSTAT(stats.thisFrame.numDrawCalls);

			D3D::context->GSSetShader(nullptr, nullptr, 0);
			((DX11::Renderer*)g_renderer)->RestoreCull();
		}
	}
}

void VertexManager::vFlush(bool useDstAlpha)
{
	u32 components = VertexLoaderManager::GetCurrentVertexFormat()->m_components;

	if (!PixelShaderCache::SetShader(
		useDstAlpha ? DSTALPHA_DUAL_SOURCE_BLEND : DSTALPHA_NONE, components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		return;
	}

	if (!VertexShaderCache::SetShader(components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		return;
	}

	u32 stride = VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride();

	PrepareDrawBuffers(stride);

	VertexLoaderManager::GetCurrentVertexFormat()->SetupVertexPointers();
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
