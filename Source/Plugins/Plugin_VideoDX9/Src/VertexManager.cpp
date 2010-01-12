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

#include "Statistics.h"
#include "Profiler.h"
#include "VertexManager.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "Utils.h"
#include "NativeVertexFormat.h"
#include "NativeVertexWriter.h"
#include "TextureCache.h"

#include "BPStructs.h"
#include "XFStructs.h"

#include "debugger/debugger.h"


using namespace D3D;

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

namespace VertexManager
{

static int lastPrimitive;

static u8 *LocalVBuffer;   
static u16 *TIBuffer;
static u16 *LIBuffer;  
static u16 *PIBuffer;  
#define MAXVBUFFERSIZE 0x50000
#define MAXIBUFFERSIZE 0xFFFF
static bool Flushed=false;

void CreateDeviceObjects();
void DestroyDeviceObjects();

bool Init()
{
	LocalVBuffer = new u8[MAXVBUFFERSIZE];
	TIBuffer = new u16[MAXIBUFFERSIZE];
	LIBuffer = new u16[MAXIBUFFERSIZE];
	PIBuffer = new u16[MAXIBUFFERSIZE];
	s_pCurBufferPointer = LocalVBuffer;
	Flushed=false;
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
	delete [] LocalVBuffer;
	delete [] TIBuffer;
	delete [] LIBuffer;
	delete [] PIBuffer;	
	ResetBuffer();
}

void CreateDeviceObjects()
{
	
}

void DestroyDeviceObjects()
{
}

void AddIndices(int _primitive, int _numVertices)
{
	switch (_primitive)
	{
	case GX_DRAW_QUADS:          IndexGenerator::AddQuads(_numVertices);     return;    
	case GX_DRAW_TRIANGLES:      IndexGenerator::AddList(_numVertices);      return;    
	case GX_DRAW_TRIANGLE_STRIP: IndexGenerator::AddStrip(_numVertices);     return;
	case GX_DRAW_TRIANGLE_FAN:   IndexGenerator::AddFan(_numVertices);       return;
	case GX_DRAW_LINE_STRIP:     IndexGenerator::AddLineStrip(_numVertices); return;
	case GX_DRAW_LINES:		     IndexGenerator::AddLineList(_numVertices);  return;
	case GX_DRAW_POINTS:         IndexGenerator::AddPoints(_numVertices);    return;
	}
}



int GetRemainingSize()
{
	return  MAXVBUFFERSIZE - (int)(s_pCurBufferPointer - LocalVBuffer);
}

void AddVertices(int _primitive, int _numVertices)
{
	if (_numVertices < 0)
		return;
	switch (_primitive)
	{
		case GX_DRAW_QUADS:          
		case GX_DRAW_TRIANGLES:      
		case GX_DRAW_TRIANGLE_STRIP: 
		case GX_DRAW_TRIANGLE_FAN:   
			if(MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen() < 2 * _numVertices)
			Flush();
			break;
		case GX_DRAW_LINE_STRIP:
		case GX_DRAW_LINES:
			if(MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen() < 2 * _numVertices)
			Flush();
			break;
		case GX_DRAW_POINTS:
			if(MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen() < _numVertices)
			Flush();
			break;
		default: return;
	}
	if(Flushed)
	{
		IndexGenerator::Start(TIBuffer,LIBuffer,PIBuffer);
		Flushed=false;
	}
	lastPrimitive = _primitive;	
	ADDSTAT(stats.thisFrame.numPrims, _numVertices);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	AddIndices(_primitive, _numVertices);
}

inline void DumpBadShaders()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	std::string error_shaders;
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
	PanicAlert("DrawIndexedPrimitiveUP failed. Shaders written to %s", filename);
#endif
}

inline void Draw(int stride)
{
	if(IndexGenerator::GetNumTriangles() > 0)
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
	if(IndexGenerator::GetNumLines() > 0)
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
	if(IndexGenerator::GetNumPoints() > 0)
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

void Flush()
{
	if (LocalVBuffer == s_pCurBufferPointer) return;	
	if(Flushed) return;
	Flushed=true;

	DVSTARTPROFILE();
	
	u32 usedtextures = 0;
	for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i) {
		if (bpmem.tevorders[i/2].getEnable(i & 1))
			usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);
	}

	if (bpmem.genMode.numindstages > 0) {
		for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i) {
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages) {
				usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);
			}
		}
	}

	u32 nonpow2tex = 0;
	for (int i = 0; i < 8; i++)
	{
		if (usedtextures & (1 << i)) {
			Renderer::SetSamplerState(i & 3, i >> 2);
			FourTexUnits &tex = bpmem.tex[i >> 2];
			TextureCache::TCacheEntry* tentry = TextureCache::Load(i, 
				(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
				tex.texTlut[i&3].tlut_format);

			if (tentry) {
				PixelShaderManager::SetTexDims(i, tentry->w, tentry->h, 0, 0);
				if (tentry->scaleX != 1.0f || tentry->scaleY != 1.0f)
					PixelShaderManager::SetCustomTexScale(i, tentry->scaleX, tentry->scaleY);
			}
			else
			{
				DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to load texture\n");});
				ERROR_LOG(VIDEO, "error loading texture");
			}

		}
	}
	PixelShaderManager::SetTexturesUsed(0);

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants();

	if (!PixelShaderCache::SetShader(false))
	{
		DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		goto shader_fail;
	}
	if (!VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
	{
		DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set vertex shader\n");});
		goto shader_fail;

	}

	int stride = g_nativeVertexFmt->GetVertexStride();
	g_nativeVertexFmt->SetupVertexPointers();
	
	Draw(stride);

	if (bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate) 
	{
		DWORD write = 0;
		if (!PixelShaderCache::SetShader(true))
		{
			DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
			goto shader_fail;
		}

		// update alpha only
		D3D::dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA);
		D3D::dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
		
		Draw(stride);

		D3D::RefreshRenderState(D3DRS_COLORWRITEENABLE);
		D3D::RefreshRenderState(D3DRS_ALPHABLENDENABLE);		
	}
	DEBUGGER_PAUSE_AT(NEXT_FLUSH,true);

shader_fail:
	ResetBuffer();
	
}

}  // namespace
