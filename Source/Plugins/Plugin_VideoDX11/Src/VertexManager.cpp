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
#include "Statistics.h"
#include "TextureCacheBase.h"
#include "VertexShaderManager.h"
#include "VideoConfig.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace DX11
{

// TODO: Find sensible values for these two
const UINT IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE*2 * 16;
const UINT VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 16;

void VertexManager::CreateDeviceObjects()
{
	D3D11_BUFFER_DESC bufdesc = CD3D11_BUFFER_DESC(IBUFFER_SIZE,
		D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, NULL, &m_indexBuffer)),
		"Failed to create index buffer.");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_indexBuffer, "index buffer of VertexManager");

	bufdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufdesc.ByteWidth = VBUFFER_SIZE;

	CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, NULL, &m_vertexBuffer)),
		"Failed to create vertex buffer.");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)m_vertexBuffer, "vertex buffer of VertexManager");

	m_indexBufferCursor = 0;
	m_vertexBufferCursor = 0;
	m_vertexDrawOffset = 0;

	m_triangleDrawIndex = 0;
	m_lineDrawIndex = 0;
	m_pointDrawIndex = 0;
}

void VertexManager::DestroyDeviceObjects()
{
	SAFE_RELEASE(m_vertexBuffer);
	SAFE_RELEASE(m_indexBuffer);
}

VertexManager::VertexManager()
{
	CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
	DestroyDeviceObjects();
}

void VertexManager::LoadBuffers()
{
	D3D11_MAPPED_SUBRESOURCE map;

	UINT vSize = UINT(s_pCurBufferPointer - LocalVBuffer);
	if (m_vertexBufferCursor + vSize >= VBUFFER_SIZE)
	{
		// Wrap around
		D3D::context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		m_vertexBufferCursor = 0;
	}
	else
	{
		// Append data
		D3D::context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
	}
	memcpy((u8*)map.pData + m_vertexBufferCursor, LocalVBuffer, vSize);
	D3D::context->Unmap(m_vertexBuffer, 0);
	m_vertexDrawOffset = m_vertexBufferCursor;
	m_vertexBufferCursor += vSize;

	UINT iCount = IndexGenerator::GetTriangleindexLen() +
		IndexGenerator::GetLineindexLen() + IndexGenerator::GetPointindexLen();
	if (m_indexBufferCursor + iCount >= IBUFFER_SIZE/2)
	{
		// Wrap around
		D3D::context->Map(m_indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		m_indexBufferCursor = 0;
	}
	else
	{
		// Append data
		D3D::context->Map(m_indexBuffer, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);
	}
	m_triangleDrawIndex = m_indexBufferCursor;
	m_lineDrawIndex = m_triangleDrawIndex + IndexGenerator::GetTriangleindexLen();
	m_pointDrawIndex = m_lineDrawIndex + IndexGenerator::GetLineindexLen();
	memcpy((u16*)map.pData + m_triangleDrawIndex, TIBuffer, 2*IndexGenerator::GetTriangleindexLen());
	memcpy((u16*)map.pData + m_lineDrawIndex, LIBuffer, 2*IndexGenerator::GetLineindexLen());
	memcpy((u16*)map.pData + m_pointDrawIndex, PIBuffer, 2*IndexGenerator::GetPointindexLen());
	D3D::context->Unmap(m_indexBuffer, 0);
	m_indexBufferCursor += iCount;
}

void VertexManager::Draw(UINT stride)
{
	D3D::context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &m_vertexDrawOffset);
	D3D::context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R16_UINT, 0);

	if (IndexGenerator::GetNumTriangles() > 0)
	{
		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D::context->DrawIndexed(IndexGenerator::GetTriangleindexLen(), m_triangleDrawIndex, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumLines() > 0)
	{
		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		D3D::context->DrawIndexed(IndexGenerator::GetLineindexLen(), m_lineDrawIndex, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumPoints() > 0)
	{
		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		D3D::context->DrawIndexed(IndexGenerator::GetPointindexLen(), m_pointDrawIndex, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
}

void VertexManager::vFlush()
{
	if (LocalVBuffer == s_pCurBufferPointer) return;
	if (Flushed) return;
	Flushed=true;
	VideoFifo_CheckEFBAccess();

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
				(tex.texMode0[i&3].min_filter & 3) && (tex.texMode0[i&3].min_filter != 8) && g_ActiveConfig.bUseNativeMips,
				(tex.texMode1[i&3].max_lod >> 4));

			if (tentry)
			{
				// 0s are probably for no manual wrapping needed.
				PixelShaderManager::SetTexDims(i, tentry->realW, tentry->realH, 0, 0);
			}
			else
				ERROR_LOG(VIDEO, "error loading texture");
		}
	}

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	bool useDstAlpha = bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate &&
		bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;

	if (!PixelShaderCache::SetShader(
		useDstAlpha ? DSTALPHA_DUAL_SOURCE_BLEND : DSTALPHA_NONE,
		g_nativeVertexFmt->m_components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		goto shader_fail;
	}
	if (!VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		goto shader_fail;
	}

	unsigned int stride = g_nativeVertexFmt->GetVertexStride();
	g_nativeVertexFmt->SetupVertexPointers();

	g_renderer->ApplyState(useDstAlpha);
	LoadBuffers();
	Draw(stride);

	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

	g_renderer->RestoreState();

shader_fail:
	ResetBuffer();
}

}  // namespace
