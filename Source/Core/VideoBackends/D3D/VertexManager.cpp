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
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace DX11
{

// TODO: Find sensible values for these two
const UINT IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 8;
const UINT VBUFFER_SIZE = 1u<<20u;//VertexManager::MAXVBUFFERSIZE;

void VertexManager::CreateDeviceObjects()
{
	CD3D11_BUFFER_DESC bufdesc(VBUFFER_SIZE, D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_INDEX_BUFFER,
	                           D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	for (auto &p : m_buffers)
	{
		p = nullptr;
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, nullptr, &p)),
		"Failed to create index buffer.");
		D3D::SetDebugObjectName((ID3D11DeviceChild*)p, "index buffer of VertexManager");
	}
	m_currentBuffer = u32(m_buffers.size()) - 1;
	m_bufferCursor = VBUFFER_SIZE;

	m_lineShader.Init();
	m_pointShader.Init();
}

void VertexManager::DestroyDeviceObjects()
{
	m_pointShader.Shutdown();
	m_lineShader.Shutdown();
	for (auto& p : m_buffers)
	{
		SAFE_RELEASE(p);
	}
}

VertexManager::VertexManager()
{
	// TODO: Fix MAXVBUFFERSIZE versus VBUFFER_SIZE and check overflow system in Common
	LocalVBuffer.resize(MAXVBUFFERSIZE + 16);
	s_pCurBufferPointer = s_pBaseBufferPointer = LocalVBuffer.data() + 16;
	s_pEndBufferPointer = LocalVBuffer.data() + LocalVBuffer.size();

	LocalIBuffer.resize(MAXIBUFFERSIZE);

	CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
	DestroyDeviceObjects();
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	UINT vSize = UINT(s_pCurBufferPointer - s_pBaseBufferPointer);
	UINT iSize = IndexGenerator::GetIndexLen() * sizeof(u16);

	u32 cursor = m_bufferCursor;
	u32 padding = cursor % stride;
	if (padding)
	{
		cursor += stride - padding;
	}
	cursor += vSize;

	cursor = (cursor + 0xf) & ~0xf;

	cursor += iSize;

	padding = cursor % 16;
	if (padding)
	{
		cursor += 16 - padding;
	}

	auto mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (cursor > VBUFFER_SIZE)
	{
		m_currentBuffer = (m_currentBuffer + 1) % m_buffers.size();
		m_bufferCursor = 0;
		mapType = D3D11_MAP_WRITE_DISCARD;
	}

	auto vbstart = m_bufferCursor;
	padding = vbstart % stride;
	if (padding)
	{
		vbstart += stride - padding;
	}
	auto vbAlignedStart = vbstart & ~0xf;
	auto vbend = vbstart + vSize;
	vbend = (vbend + 0xf) & ~0xf;

	D3D11_MAPPED_SUBRESOURCE map;
	auto hr = D3D::context->Map(m_buffers[m_currentBuffer], 0, mapType, 0, &map);

	if (hr != S_OK)
	{
		PanicAlert("unable to lock the VB/IB buffer, prepare to see garbage on screen");
		return;
	}

	D3D11_BOX vbSrcBox { vbAlignedStart, 0, 0, vbend, 1, 1 };
	memcpy((u8*)map.pData + vbSrcBox.left, s_pBaseBufferPointer - (vbstart - vbAlignedStart), vbSrcBox.right - vbSrcBox.left);

	D3D11_BOX ibSrcBox { vbend, 0, 0, (vbend + iSize + 0xF) & ~0xf, 1, 1 };
	memcpy((u8*)map.pData + ibSrcBox.left, GetIndexBuffer(), ibSrcBox.right - ibSrcBox.left);

	D3D::context->Unmap(m_buffers[m_currentBuffer], 0);

	m_bufferCursor = ibSrcBox.right;
	m_vertexDrawOffset = vbstart/stride;
	m_indexDrawOffset = ibSrcBox.left / 2;

	ADDSTAT(stats.thisFrame.bytesVertexStreamed, vbSrcBox.right - vbSrcBox.left);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, ibSrcBox.right - ibSrcBox.left);
}

static const float LINE_PT_TEX_OFFSETS[8] = {
	0.f, 0.0625f, 0.125f, 0.25f, 0.5f, 1.f, 1.f, 1.f
};

void VertexManager::Draw(UINT stride)
{
	u32 zero = 0;
	D3D::context->IASetVertexBuffers(0, 1, &m_buffers[m_currentBuffer], &stride, &zero);
	D3D::context->IASetIndexBuffer(m_buffers[m_currentBuffer], DXGI_FORMAT_R16_UINT, 0);

	if (current_primitive_type == PRIMITIVE_TRIANGLES)
	{
		auto pt = g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ?
		          D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP :
		          D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		D3D::context->IASetPrimitiveTopology(pt);
		D3D::context->DrawIndexed(IndexGenerator::GetIndexLen(), m_indexDrawOffset, m_vertexDrawOffset);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
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
			D3D::context->DrawIndexed(IndexGenerator::GetIndexLen(), m_indexDrawOffset, m_vertexDrawOffset);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);

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
			D3D::context->DrawIndexed(IndexGenerator::GetIndexLen(), m_indexDrawOffset, m_vertexDrawOffset);
			INCSTAT(stats.thisFrame.numIndexedDrawCalls);

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
	unsigned int stride = g_nativeVertexFmt->GetVertexStride();
	PrepareDrawBuffers(stride);
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
