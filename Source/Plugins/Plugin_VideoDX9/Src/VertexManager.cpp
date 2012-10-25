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
#include "Fifo.h"
#include "Statistics.h"
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
#include "Debugger.h"
#include "VideoConfig.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;
namespace DX9
{
//This are the initially requeted size for the buffers expresed in elements
const u32 IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * 16;
const u32 VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE * 16;
const u32 MAXVBUFFER_COUNT = 2;

inline void DumpBadShaders()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	// TODO: Reimplement!
/*	std::string error_shaders;
	error_shaders.append(VertexShaderCache::GetCurrentShaderCode());
	error_shaders.append(PixelShaderCache::GetCurrentShaderCode());
	char filename[512] = "bad_shader_combo_0.txt";
	int which = 0;
	while (File::Exists(filename))
	{
		which++;
		sprintf(filename, "bad_shader_combo_%i.txt", which);
	}
	File::WriteStringToFile(true, error_shaders, filename);
	PanicAlert("DrawIndexedPrimitiveUP failed. Shaders written to %s", filename);*/
#endif
}

void VertexManager::CreateDeviceObjects()
{
	NumVBuffers = 0;
	VBuffers = NULL;
	IBuffers = NULL;
	D3DCAPS9 DeviceCaps = D3D::GetCaps();
	u32 devicevMaxBufferSize =  DeviceCaps.MaxPrimitiveCount * 3 * DeviceCaps.MaxStreamStride;
	//Calculate Device Dependant size
	CurrentVBufferSize = (VBUFFER_SIZE > devicevMaxBufferSize) ? devicevMaxBufferSize : VBUFFER_SIZE;
	CurrentIBufferSize = (IBUFFER_SIZE > DeviceCaps.MaxVertexIndex) ? DeviceCaps.MaxVertexIndex : IBUFFER_SIZE;
	//if device caps are not enough for Vbuffer fall back to vertex arrays
	if (CurrentIBufferSize < MAXIBUFFERSIZE || CurrentVBufferSize < MAXVBUFFERSIZE) return;
	
	VBuffers = new LPDIRECT3DVERTEXBUFFER9[MAXVBUFFER_COUNT];
	IBuffers = new LPDIRECT3DINDEXBUFFER9[MAXVBUFFER_COUNT];

	bool Fail = false;
	for (CurrentVBuffer = 0; CurrentVBuffer < MAXVBUFFER_COUNT; CurrentVBuffer++)
	{
		VBuffers[CurrentVBuffer] = NULL;
		IBuffers[CurrentVBuffer] = NULL;
	}
	for (CurrentVBuffer = 0; CurrentVBuffer < MAXVBUFFER_COUNT; CurrentVBuffer++)
	{
		if(FAILED( D3D::dev->CreateVertexBuffer( CurrentVBufferSize,  D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &VBuffers[CurrentVBuffer], NULL ) ) )
		{
			Fail = true;
			break;
		}
		if( FAILED( D3D::dev->CreateIndexBuffer( CurrentIBufferSize * sizeof(u16),  D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &IBuffers[CurrentVBuffer], NULL ) ) )
		{
			Fail = true;
			return;
		}
	}
	NumVBuffers = CurrentVBuffer;
	CurrentVBuffer = 0;
	CurrentIBuffer = 0;
	CurrentIBufferIndex = CurrentIBufferSize;
	CurrentVBufferIndex = CurrentVBufferSize;
	
	if (Fail)
	{
		NumVBuffers--;
		if (NumVBuffers < 2)
		{
			//Error creating Vertex buffers. clean and fall to Vertex arrays
			NumVBuffers = MAXVBUFFER_COUNT;
			DestroyDeviceObjects();
		}		
	}	
}
void VertexManager::DestroyDeviceObjects()
{
	D3D::dev->SetStreamSource( 0, NULL, 0, 0);
	D3D::dev->SetIndices(NULL);
	for (int i = 0; i < MAXVBUFFER_COUNT; i++)
	{
		if(VBuffers)
		{
			if (VBuffers[i])
			{
				VBuffers[i]->Release();
				VBuffers[i] = NULL;
			}
		}

		if (IBuffers[i])
		{
			IBuffers[i]->Release();
			IBuffers[i] = NULL;
		}
	}
	if(VBuffers)
		delete [] VBuffers;
	if(IBuffers)
		delete [] IBuffers;
	VBuffers = NULL;
	IBuffers = NULL;
}

void VertexManager::PrepareVBuffers(int stride)
{
	if (!NumVBuffers)
	{
		return;
	}
	u8* pVertices;
	u16* pIndices;
	int datasize = IndexGenerator::GetNumVerts() * stride;
	int TdataSize = IndexGenerator::GetTriangleindexLen();
	int LDataSize = IndexGenerator::GetLineindexLen();
	int PDataSize = IndexGenerator::GetPointindexLen();
	int IndexDataSize = TdataSize + LDataSize + PDataSize;
	DWORD LockMode = D3DLOCK_NOOVERWRITE;

	if (CurrentVBufferIndex > CurrentVBufferSize - datasize)
	{
		LockMode = D3DLOCK_DISCARD;
		CurrentVBufferIndex = 0;
		CurrentVBuffer = (CurrentVBuffer + 1) % NumVBuffers;
	}

	if(FAILED(VBuffers[CurrentVBuffer]->Lock(CurrentVBufferIndex, datasize,(VOID**)(&pVertices), LockMode))) 
	{
		DestroyDeviceObjects();
		return;
	}
	memcpy(pVertices, LocalVBuffer, datasize);
	VBuffers[CurrentVBuffer]->Unlock();

	LockMode = D3DLOCK_NOOVERWRITE;

	if (CurrentIBufferIndex > CurrentIBufferSize - IndexDataSize)
	{
		LockMode = D3DLOCK_DISCARD;
		CurrentIBufferIndex = 0;
		CurrentIBuffer = (CurrentIBuffer + 1) % NumVBuffers;
	}
	
	if(FAILED(IBuffers[CurrentIBuffer]->Lock(CurrentIBufferIndex * sizeof(u16), IndexDataSize * sizeof(u16), (VOID**)(&pIndices), LockMode ))) 
	{
		DestroyDeviceObjects();
		return;
	}
	if(TdataSize)
	{		
		memcpy(pIndices, TIBuffer, TdataSize * sizeof(u16));
		pIndices += TdataSize;
	}
	if(LDataSize)
	{		
		memcpy(pIndices, LIBuffer, LDataSize * sizeof(u16));
		pIndices += LDataSize;
	}
	if(PDataSize)
	{		
		memcpy(pIndices, PIBuffer, PDataSize * sizeof(u16));
	}
	IBuffers[CurrentIBuffer]->Unlock();
	D3D::dev->SetStreamSource( 0, VBuffers[CurrentVBuffer], CurrentVBufferIndex, stride);
	if(CurrentIBufferIndex == 0)
	{
		D3D::dev->SetIndices(IBuffers[CurrentIBuffer]);
	}
}

void VertexManager::DrawVB(int stride)
{
	if (IndexGenerator::GetNumTriangles() > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitive(
			D3DPT_TRIANGLELIST, 
			0,
			0, 
			IndexGenerator::GetNumVerts(),
			CurrentIBufferIndex, 
			IndexGenerator::GetNumTriangles())))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumLines() > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitive(
			D3DPT_LINELIST, 
			0,
			0, 
			IndexGenerator::GetNumVerts(),
			CurrentIBufferIndex + IndexGenerator::GetTriangleindexLen(), 
			IndexGenerator::GetNumLines())))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumPoints() > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitive(
			D3DPT_POINTLIST, 
			0,
			0, 
			IndexGenerator::GetNumVerts(),
			CurrentIBufferIndex + IndexGenerator::GetTriangleindexLen() + IndexGenerator::GetLineindexLen(), 
			IndexGenerator::GetNumPoints())))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	
}

void VertexManager::DrawVA(int stride)
{	
	if (IndexGenerator::GetNumTriangles() > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitiveUP(
			D3DPT_TRIANGLELIST, 
			0, IndexGenerator::GetNumVerts(), IndexGenerator::GetNumTriangles(), 
			TIBuffer, 
			D3DFMT_INDEX16, 
			LocalVBuffer, 
			stride)))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumLines() > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitiveUP(
			D3DPT_LINELIST, 
			0, IndexGenerator::GetNumVerts(), IndexGenerator::GetNumLines(), 
			LIBuffer, 
			D3DFMT_INDEX16, 
			LocalVBuffer, 
			stride)))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (IndexGenerator::GetNumPoints() > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitiveUP(
			D3DPT_POINTLIST, 
			0, IndexGenerator::GetNumVerts(), IndexGenerator::GetNumPoints(), 
			PIBuffer, 
			D3DFMT_INDEX16, 
			LocalVBuffer, 
			stride)))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}	
}

void VertexManager::vFlush()
{
	if (LocalVBuffer == s_pCurBufferPointer) return;
	if (Flushed) return;
	Flushed = true;
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
			FourTexUnits &tex = bpmem.tex[i >> 2];
			TextureCache::TCacheEntryBase* tentry = TextureCache::Load(i, 
				(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
				tex.texTlut[i&3].tlut_format,
				(tex.texMode0[i&3].min_filter & 3) && (tex.texMode0[i&3].min_filter != 8),
				tex.texMode1[i&3].max_lod >> 4,
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
	PixelShaderManager::SetConstants();
	int stride = g_nativeVertexFmt->GetVertexStride();
	if (!PixelShaderCache::SetShader(DSTALPHA_NONE,g_nativeVertexFmt->m_components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		goto shader_fail;
	}
	if (!VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set vertex shader\n");});
		goto shader_fail;

	}
	PrepareVBuffers(stride);
	g_nativeVertexFmt->SetupVertexPointers();	
	if(NumVBuffers){ DrawVB(stride);} else { DrawVA(stride);}

	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate &&
						bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;
	if (useDstAlpha)
	{
		if (!PixelShaderCache::SetShader(DSTALPHA_ALPHA_PASS, g_nativeVertexFmt->m_components))
		{
			GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
			goto shader_fail;
		}
		// update alpha only
		g_renderer->ApplyState(true);
		if(NumVBuffers){ DrawVB(stride);} else { DrawVA(stride);}
		g_renderer->RestoreState();
	}
	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

shader_fail:
	if(NumVBuffers)
	{
		CurrentIBufferIndex += IndexGenerator::GetTriangleindexLen() + IndexGenerator::GetLineindexLen() + IndexGenerator::GetPointindexLen();
		CurrentVBufferIndex += IndexGenerator::GetNumVerts() * stride;
	}
	ResetBuffer();
}

}
