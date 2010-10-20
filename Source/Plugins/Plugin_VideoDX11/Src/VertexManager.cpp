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

#include "Common.h"
#include "FileUtil.h"

#include "D3DBase.h"
#include "D3DShader.h"
#include "D3DUtil.h"
#include "Fifo.h"
#include "Statistics.h"
#include "Profiler.h"
#include "FramebufferManager.h"
#include "VertexManager.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "NativeVertexFormat.h"
#include "TextureCache.h"
#include "main.h"

#include "BPStructs.h"
#include "XFStructs.h"

#include "Globals.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace DX11
{

// TODO: find sensible values for these two
enum
{
	NUM_VERTEXBUFFERS = 8,
	NUM_INDEXBUFFERS = 10,
};

ID3D11Buffer* indexbuffers[NUM_INDEXBUFFERS] = {};
ID3D11Buffer* vertexbuffers[NUM_VERTEXBUFFERS] = {};

inline ID3D11Buffer* GetSuitableIndexBuffer(const u32 minsize)
{
	for (u32 k = 0; k < NUM_INDEXBUFFERS-1; ++k)
		if (minsize > 2 * (((u32)VertexManager::MAXIBUFFERSIZE)>>(k+1)))
			return indexbuffers[k];
	return indexbuffers[NUM_INDEXBUFFERS-1];
}

inline ID3D11Buffer* GetSuitableVertexBuffer(const u32 minsize)
{
	for (u32 k = 0; k < NUM_VERTEXBUFFERS-1; ++k)
		if (minsize > (((u32)VertexManager::MAXVBUFFERSIZE)>>(k+1)))
			return vertexbuffers[k];
	return vertexbuffers[NUM_VERTEXBUFFERS-1];
}

void CreateDeviceObjects()
{
	D3D11_BUFFER_DESC bufdesc = CD3D11_BUFFER_DESC(VertexManager::MAXIBUFFERSIZE * 2,
		D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	for (u32 k = 0; k < NUM_INDEXBUFFERS; ++k, bufdesc.ByteWidth >>= 1)
	{
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, NULL, indexbuffers + k)),
			"Failed to create index buffer [%i].", k);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)indexbuffers[k], "an index buffer of VertexManager");
	}

	bufdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufdesc.ByteWidth = VertexManager::MAXVBUFFERSIZE;
	for (u32 k = 0; k < NUM_VERTEXBUFFERS; ++k, bufdesc.ByteWidth >>= 1)
	{
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, NULL, vertexbuffers + k)),
			"Failed to create vertex buffer [%i].", k);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)vertexbuffers[k], "a vertex buffer of VertexManager");
	}
}

void DestroyDeviceObjects()
{
	for (u32 k = 0; k < NUM_INDEXBUFFERS; ++k)
		SAFE_RELEASE(indexbuffers[k]);

	for (u32 k = 0; k < NUM_VERTEXBUFFERS; ++k)
		SAFE_RELEASE(vertexbuffers[k]);
}

VertexManager::VertexManager()
{
	CreateDeviceObjects();
}

VertexManager::~VertexManager()
{
	DestroyDeviceObjects();
}

void VertexManager::Draw(u32 stride, bool alphapass)
{
	D3D11_MAPPED_SUBRESOURCE map;
	ID3D11Buffer* vertexbuffer = GetSuitableVertexBuffer((u32)(s_pCurBufferPointer - LocalVBuffer));

	if (!alphapass)
	{
		D3D::context->Map(vertexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, LocalVBuffer, (u32)(s_pCurBufferPointer - LocalVBuffer));
		D3D::context->Unmap(vertexbuffer, 0);
	}

	UINT bufoffset = 0;
	UINT bufstride = stride;

	if (!alphapass)
		D3D::gfxstate->ApplyState();
	else
		D3D::gfxstate->AlphaPass();

	if (IndexGenerator::GetNumTriangles() > 0)
	{
		u32 indexbuffersize = IndexGenerator::GetTriangleindexLen();
		ID3D11Buffer* indexbuffer = GetSuitableIndexBuffer(2*indexbuffersize);
		if (!alphapass)
		{
			D3D::context->Map(indexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			memcpy(map.pData, TIBuffer, 2*indexbuffersize);
			D3D::context->Unmap(indexbuffer, 0);
		}

		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D::context->IASetVertexBuffers(0, 1, &vertexbuffer, &bufstride, &bufoffset);
		D3D::context->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R16_UINT, 0);

		D3D::context->DrawIndexed(indexbuffersize, 0, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumLines() > 0)
	{
		u32 indexbuffersize = IndexGenerator::GetLineindexLen();
		ID3D11Buffer* indexbuffer = GetSuitableIndexBuffer(2*indexbuffersize);
		if (!alphapass)
		{
			D3D::context->Map(indexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			memcpy(map.pData, LIBuffer, 2*indexbuffersize);
			D3D::context->Unmap(indexbuffer, 0);
		}

		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		D3D::context->IASetVertexBuffers(0, 1, &vertexbuffer, &bufstride, &bufoffset);
		D3D::context->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R16_UINT, 0);

		D3D::context->DrawIndexed(indexbuffersize, 0, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumPoints() > 0)
	{
		u32 indexbuffersize = IndexGenerator::GetPointindexLen();
		ID3D11Buffer* indexbuffer = GetSuitableIndexBuffer(2*indexbuffersize);
		if (!alphapass)
		{
			D3D::context->Map(indexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			memcpy(map.pData, PIBuffer, 2*indexbuffersize);
			D3D::context->Unmap(indexbuffer, 0);
		}

		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		D3D::context->IASetVertexBuffers(0, 1, &vertexbuffer, &bufstride, &bufoffset);
		D3D::context->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R16_UINT, 0);

		D3D::context->DrawIndexed(indexbuffersize, 0, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
}

void VertexManager::vFlush()
{
	if (LocalVBuffer == s_pCurBufferPointer) return;
	if (Flushed) return;
	Flushed=true;
	VideoFifo_CheckEFBAccess();

	DVSTARTPROFILE();

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
			Renderer::SetSamplerState(i & 3, i >> 2);
			FourTexUnits &tex = bpmem.tex[i >> 2];
			TextureCache::TCacheEntryBase* tentry = TextureCache::Load(i, 
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

	if (!PixelShaderCache::SetShader(false,g_nativeVertexFmt->m_components))
		goto shader_fail;
	if (!VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
		goto shader_fail;

	unsigned int stride = g_nativeVertexFmt->GetVertexStride();
	g_nativeVertexFmt->SetupVertexPointers();

	Draw(stride, false);

	if (bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate)
	{
		DWORD write = 0;
		if (!PixelShaderCache::SetShader(true,g_nativeVertexFmt->m_components))
			goto shader_fail;

		// update alpha only
		Draw(stride, true);
	}
	D3D::gfxstate->Reset();

shader_fail:
	ResetBuffer();
}

}  // namespace
