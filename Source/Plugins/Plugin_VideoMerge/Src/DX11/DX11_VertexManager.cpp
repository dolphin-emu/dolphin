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

#include <string>

// Common
#include "Common.h"
#include "FileUtil.h"

// VideoCommon
#include "BPStructs.h"
#include "XFStructs.h"
#include "Fifo.h"
#include "Statistics.h"
#include "Profiler.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "NativeVertexFormat.h"
#include "NativeVertexWriter.h"

// DX11
#include "DX11_D3DBase.h"
#include "DX11_D3DShader.h"
#include "DX11_D3DUtil.h"
#include "DX11_VertexManager.h"
#include "DX11_VertexShaderCache.h"
#include "DX11_PixelShaderCache.h"
#include "DX11_TextureCache.h"
#include "DX11_FramebufferManager.h"
#include "DX11_Render.h"

#include "../Main.h"

//using std::string;

namespace DX11
{

using namespace D3D;

ID3D11Buffer* indexbuffers[NUM_INDEXBUFFERS] = {};
ID3D11Buffer* vertexbuffers[NUM_VERTEXBUFFERS] = {};

// TODO: these seem ugly
inline ID3D11Buffer* GetSuitableIndexBuffer(const u32 minsize)
{
	for (u32 k = 0; k < NUM_INDEXBUFFERS - 1; ++k)
		if (minsize > (((u32)MAXIBUFFERSIZE) >> k))
			return indexbuffers[k];
	return indexbuffers[NUM_INDEXBUFFERS - 1];
}

inline ID3D11Buffer* GetSuitableVertexBuffer(const u32 minsize)
{
	for (u32 k = 0; k < NUM_VERTEXBUFFERS - 1; ++k)
		if (minsize > (((u32)MAXVBUFFERSIZE) >> (k + 1)))
			return vertexbuffers[k];
	return vertexbuffers[NUM_VERTEXBUFFERS - 1];
}

void CreateDeviceObjects()
{
	D3D11_BUFFER_DESC bufdesc = CD3D11_BUFFER_DESC(MAXIBUFFERSIZE * 2,
		D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	for (u32 k = 0; k < NUM_INDEXBUFFERS; ++k, bufdesc.ByteWidth >>= 1)
	{
		CHECK(SUCCEEDED(D3D::device->CreateBuffer(&bufdesc, NULL, indexbuffers + k)),
			"Failed to create index buffer [%i].", k);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)indexbuffers[k], "an index buffer of VertexManager");
	}

	bufdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufdesc.ByteWidth = MAXVBUFFERSIZE;
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
	ResetBuffer();
}

void VertexManager::Draw(u32 stride, bool alphapass)
{
	static const UINT bufoffset = 0;
	const UINT bufstride = stride;

	D3D11_MAPPED_SUBRESOURCE map;
	ID3D11Buffer* const vertexbuffer = GetSuitableVertexBuffer((u32)(s_pCurBufferPointer - LocalVBuffer));

	if (alphapass)
	{
		gfxstate->AlphaPass();
	}
	else
	{
		context->Map(vertexbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, LocalVBuffer, (u32)(s_pCurBufferPointer - LocalVBuffer));
		context->Unmap(vertexbuffer, 0);

		gfxstate->ApplyState();
	}

	D3D::context->IASetVertexBuffers(0, 1, &vertexbuffer, &bufstride, &bufoffset);

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
		D3D::context->IASetIndexBuffer(indexbuffer, DXGI_FORMAT_R16_UINT, 0);

		D3D::context->DrawIndexed(indexbuffersize, 0, 0);
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
}

}
