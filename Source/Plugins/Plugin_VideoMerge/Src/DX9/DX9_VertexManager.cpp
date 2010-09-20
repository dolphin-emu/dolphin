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

#include "DX9_D3DBase.h"
#include "Fifo.h"
#include "Statistics.h"
#include "Profiler.h"
#include "DX9_VertexManager.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "DX9_VertexShaderCache.h"
#include "PixelShaderManager.h"
#include "DX9_PixelShaderCache.h"
#include "NativeVertexFormat.h"
#include "NativeVertexWriter.h"
#include "DX9_TextureCache.h"

#include "../Main.h"

#include "BPStructs.h"
#include "XFStructs.h"

//#include "debugger/debugger.h"

namespace DX9
{

using namespace D3D;

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;

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

void VertexManager::Draw(u32 stride, bool alphapass)
{
	if (alphapass)
	{
		DWORD write = 0;
		if (false == g_pixel_shader_cache->SetShader(true))
		{
			//DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
			//goto shader_fail;
			return;
		}
		// update alpha only
		D3D::ChangeRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA);
		D3D::ChangeRenderState(D3DRS_ALPHABLENDENABLE, false);
	}

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

	if (alphapass)
	{
		D3D::RefreshRenderState(D3DRS_COLORWRITEENABLE);
		D3D::RefreshRenderState(D3DRS_ALPHABLENDENABLE);
	}
}

}  // namespace
