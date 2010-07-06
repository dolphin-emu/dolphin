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
#include "FBManager.h"
#include "VertexManager.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "NativeVertexFormat.h"
#include "NativeVertexWriter.h"
#include "TextureCache.h"

#include "BPStructs.h"
#include "XFStructs.h"

#include "Globals.h"

#include <string>
using std::string;

using namespace D3D;

extern NativeVertexFormat* g_nativeVertexFmt;

namespace VertexManager
{

#define MAXVBUFFERSIZE 0x50000
#define MAXIBUFFERSIZE 0xFFFF

// TODO: find sensible values for these two
#define NUM_VERTEXBUFFERS 6
#define NUM_INDEXBUFFERS 4

int lastPrimitive;

u8* LocalVBuffer = NULL;
u16* TIBuffer = NULL;
u16* LIBuffer = NULL;
u16* PIBuffer = NULL;
bool Flushed=false;

ID3D11Buffer* indexbuffers[NUM_INDEXBUFFERS] = {NULL};
ID3D11Buffer* vertexbuffers[NUM_VERTEXBUFFERS] = {NULL};
ID3D11Buffer* lineindexbuffer = NULL;
ID3D11Buffer* pointindexbuffer = NULL;

inline ID3D11Buffer* GetSuitableIndexBuffer(const unsigned int minsize)
{
	for (unsigned int k = 0;k < NUM_INDEXBUFFERS-1;k++)
		if (minsize > 2*(unsigned int)MAXIBUFFERSIZE>>(k+1))
			return indexbuffers[k];

	return indexbuffers[NUM_INDEXBUFFERS-1];
}

inline ID3D11Buffer* GetSuitableVertexBuffer(const unsigned int minsize)
{
	for (unsigned int k = 0;k < NUM_VERTEXBUFFERS-1;++k)
		if (minsize > (unsigned int)MAXVBUFFERSIZE>>(k+1))
			return vertexbuffers[k];

	return vertexbuffers[NUM_VERTEXBUFFERS-1];
}

void CreateDeviceObjects()
{
	D3D11_BUFFER_DESC bufdesc;
	HRESULT hr;
	for (unsigned int k = 0;k < NUM_INDEXBUFFERS;++k)
	{
		bufdesc = CD3D11_BUFFER_DESC(2*MAXIBUFFERSIZE >> k, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		hr = D3D::device->CreateBuffer(&bufdesc, NULL, &indexbuffers[k]);
		if (FAILED(hr)) PanicAlert("Failed to create index buffer, %s %d\n", __FILE__, __LINE__);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)indexbuffers[k], "an index buffer of VertexManager");
	}

	bufdesc = CD3D11_BUFFER_DESC(2*MAXIBUFFERSIZE, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	hr = D3D::device->CreateBuffer(&bufdesc, NULL, &lineindexbuffer);
	if (FAILED(hr)) PanicAlert("Failed to create index buffer, %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)lineindexbuffer, "line index buffer of VertexManager");
	hr = D3D::device->CreateBuffer(&bufdesc, NULL, &pointindexbuffer);
	if (FAILED(hr)) PanicAlert("Failed to create index buffer, %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)pointindexbuffer, "point index buffer of VertexManager");

	for (unsigned int k = 0;k < NUM_VERTEXBUFFERS;++k)
	{
		bufdesc = CD3D11_BUFFER_DESC(MAXVBUFFERSIZE >> k, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		hr = D3D::device->CreateBuffer(&bufdesc, NULL, &vertexbuffers[k]);
		if (FAILED(hr)) PanicAlert("Failed to create vertex buffer, %s %d\n", __FILE__, __LINE__);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)vertexbuffers[k], "a vertex buffer of VertexManager");
	}
}

void DestroyDeviceObjects()
{
	SAFE_RELEASE(lineindexbuffer);
	SAFE_RELEASE(pointindexbuffer);
	for (unsigned int k = 0;k < NUM_INDEXBUFFERS;++k)
		SAFE_RELEASE(indexbuffers[k]);

	for (unsigned int k = 0;k < NUM_VERTEXBUFFERS;++k)
		SAFE_RELEASE(vertexbuffers[k]);
}

bool Init()
{
	LocalVBuffer = new u8[MAXVBUFFERSIZE];
	TIBuffer = new u16[MAXIBUFFERSIZE];
	LIBuffer = new u16[MAXIBUFFERSIZE];
	PIBuffer = new u16[MAXIBUFFERSIZE];
	s_pCurBufferPointer = LocalVBuffer;
	Flushed=false;

	CreateDeviceObjects();

	IndexGenerator::Start(TIBuffer,LIBuffer,PIBuffer);
	return true;
}

void ResetBuffer()
{
	s_pCurBufferPointer = LocalVBuffer;
}

void Shutdown()
{
	DestroyDeviceObjects();
	SAFE_DELETE_ARRAY(LocalVBuffer);
	SAFE_DELETE_ARRAY(TIBuffer);
	SAFE_DELETE_ARRAY(LIBuffer);
	SAFE_DELETE_ARRAY(PIBuffer);
	ResetBuffer();
}

void AddIndices(int _primitive, int _numVertices)
{
	switch (_primitive)
	{
		case GX_DRAW_QUADS:          IndexGenerator::AddQuads(_numVertices);     break;
		case GX_DRAW_TRIANGLES:      IndexGenerator::AddList(_numVertices);      break;
		case GX_DRAW_TRIANGLE_STRIP: IndexGenerator::AddStrip(_numVertices);     break;
		case GX_DRAW_TRIANGLE_FAN:   IndexGenerator::AddFan(_numVertices);       break;
		case GX_DRAW_LINE_STRIP:     IndexGenerator::AddLineStrip(_numVertices); break;
		case GX_DRAW_LINES:          IndexGenerator::AddLineList(_numVertices);  break;
		case GX_DRAW_POINTS:         IndexGenerator::AddPoints(_numVertices);    break;
	}
}

int GetRemainingSize()
{
	return MAXVBUFFERSIZE - (int)(s_pCurBufferPointer - LocalVBuffer);
}

int GetRemainingVertices(int primitive)
{
	switch (primitive)
	{
		case GX_DRAW_QUADS:
		case GX_DRAW_TRIANGLES:
		case GX_DRAW_TRIANGLE_STRIP:
		case GX_DRAW_TRIANGLE_FAN:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen())/3;
		case GX_DRAW_LINE_STRIP:
		case GX_DRAW_LINES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen())/2;
		case GX_DRAW_POINTS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen());
		default: return 0;
	}
}

void AddVertices(int _primitive, int _numVertices)
{
	if (_numVertices <= 0) return;

	switch (_primitive)
	{
		case GX_DRAW_QUADS:
		case GX_DRAW_TRIANGLES:
		case GX_DRAW_TRIANGLE_STRIP:
		case GX_DRAW_TRIANGLE_FAN:
			if (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen() < 3 * _numVertices)
			Flush();
			break;
		case GX_DRAW_LINE_STRIP:
		case GX_DRAW_LINES:
			if (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen() < 2 * _numVertices)
			Flush();
			break;
		case GX_DRAW_POINTS:
			if (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen() < _numVertices)
			Flush();
			break;
		default: return;
	}
	if (Flushed)
	{
		IndexGenerator::Start(TIBuffer,LIBuffer,PIBuffer);
		Flushed=false;
	}
	lastPrimitive = _primitive;
	ADDSTAT(stats.thisFrame.numPrims, _numVertices);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	AddIndices(_primitive, _numVertices);
}

inline void Draw(unsigned int stride, bool alphapass)
{
	D3D11_MAPPED_SUBRESOURCE map;
	ID3D11Buffer* vertexbuffer = GetSuitableVertexBuffer(IndexGenerator::GetNumVerts() * stride);

	if (!alphapass)
	{
		context->Map(vertexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, LocalVBuffer, IndexGenerator::GetNumVerts() * stride);
		context->Unmap(vertexbuffer, 0);
	}

	UINT bufoffset = 0;
	UINT bufstride = stride;

	if (!alphapass) gfxstate->ApplyState();
	else gfxstate->AlphaPass();
	if (IndexGenerator::GetNumTriangles() > 0)
	{
		ID3D11Buffer* indexbuffer = GetSuitableIndexBuffer(2*3*IndexGenerator::GetNumTriangles());
		if (!alphapass)
		{
			D3D::context->Map(indexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			memcpy(map.pData, TIBuffer, 2*3*IndexGenerator::GetNumTriangles());
			D3D::context->Unmap(indexbuffer, 0);
		}

		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D::context->IASetVertexBuffers(0, 1, &vertexbuffer, &bufstride, &bufoffset);
		D3D::context->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R16_UINT, 0);

		D3D::context->DrawIndexed(3*IndexGenerator::GetNumTriangles(), 0, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumLines() > 0)
	{
		if (!alphapass)
		{
			D3D::context->Map(lineindexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			memcpy(map.pData, LIBuffer, 2*2*IndexGenerator::GetNumLines());
			D3D::context->Unmap(lineindexbuffer, 0);
		}

		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		D3D::context->IASetVertexBuffers(0, 1, &vertexbuffer, &bufstride, &bufoffset);
		D3D::context->IASetIndexBuffer(lineindexbuffer, DXGI_FORMAT_R16_UINT, 0);

		D3D::context->DrawIndexed(2*IndexGenerator::GetNumLines(), 0, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumPoints() > 0)
	{
		if (!alphapass)
		{
			D3D::context->Map(pointindexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			memcpy(map.pData, PIBuffer, 2*IndexGenerator::GetNumPoints());
			D3D::context->Unmap(pointindexbuffer, 0);
		}

		D3D::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		D3D::context->IASetVertexBuffers(0, 1, &vertexbuffer, &bufstride, &bufoffset);
		D3D::context->IASetIndexBuffer(pointindexbuffer, DXGI_FORMAT_R16_UINT, 0);

		D3D::context->DrawIndexed(IndexGenerator::GetNumPoints(), 0, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
}

void Flush()
{
	if (LocalVBuffer == s_pCurBufferPointer) return;
	if (Flushed) return;
	Flushed=true;
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

	for (unsigned int i = 0; i < 8; i++)
	{
		if (usedtextures & (1 << i))
		{
			Renderer::SetSamplerState(i & 3, i >> 2);
			FourTexUnits &tex = bpmem.tex[i >> 2];
			TextureCache::TCacheEntry* tentry = TextureCache::Load(i, 
							(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
							tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
							tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
							tex.texTlut[i&3].tlut_format,
							(tex.texMode0[i&3].min_filter & 3) && (tex.texMode0[i&3].min_filter != 8) && g_ActiveConfig.bUseNativeMips,
							(tex.texMode1[i&3].max_lod >> 4));

			if (tentry)
			{
				PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, 0, 0);
				if (tentry->scaleX != 1.0f || tentry->scaleY != 1.0f)
					PixelShaderManager::SetCustomTexScale(i, tentry->scaleX, tentry->scaleY);
			}
			else
			{
				ERROR_LOG(VIDEO, "error loading texture");
			}
		}
	}
	PixelShaderManager::SetTexturesUsed(0);

	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	if (!PixelShaderCache::SetShader(false))
		goto shader_fail;
	if (!VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
		goto shader_fail;

	unsigned int stride = g_nativeVertexFmt->GetVertexStride();
	g_nativeVertexFmt->SetupVertexPointers();

	Draw(stride, false);

	if (bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate)
	{
		DWORD write = 0;
		if (!PixelShaderCache::SetShader(true))
			goto shader_fail;

		// update alpha only
		Draw(stride, true);
	}
	gfxstate->Reset();

shader_fail:
	ResetBuffer();
}

}  // namespace
