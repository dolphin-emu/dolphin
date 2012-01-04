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

#include "VideoState.h"

#include "BPMemory.h"
#include "CPMemory.h"
#include "XFMemory.h"
#include "TextureDecoder.h"
#include "Fifo.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "VertexManagerBase.h"

static void DoState(PointerWrap &p)
{
    // BP Memory
    p.Do(bpmem);
	p.DoMarker("BP Memory");

	// CP Memory
    p.DoArray(arraybases, 16);
    p.DoArray(arraystrides, 16);
    p.Do(MatrixIndexA);
    p.Do(MatrixIndexB);
    p.Do(g_VtxDesc.Hex);
	p.DoArray(g_VtxAttr, 8);
	p.DoMarker("CP Memory");

    // XF Memory
    p.Do(xfregs);
    p.DoArray(xfmem, XFMEM_SIZE);
	p.DoMarker("XF Memory");

	// Texture decoder
    p.DoArray(texMem, TMEM_SIZE);
	p.DoMarker("texMem");
 
    // FIFO
    Fifo_DoState(p);
	p.DoMarker("Fifo");

    CommandProcessor::DoState(p);
	p.DoMarker("CommandProcessor");

	PixelEngine::DoState(p);
	p.DoMarker("PixelEngine");

	// the old way of replaying current bpmem as writes to push side effects to pixel shader manager doesn't really work.
	PixelShaderManager::DoState(p);
	p.DoMarker("PixelShaderManager");

	VertexShaderManager::DoState(p);
	p.DoMarker("VertexShaderManager");

	VertexManager::DoState(p);
	p.DoMarker("VertexManager");

	// TODO: search for more data that should be saved and add it here
}

void VideoCommon_DoState(PointerWrap &p)
{
    DoState(p);
}

void VideoCommon_RunLoop(bool enable)
{
	EmulatorState(enable);
}

void VideoCommon_Init()
{
	memset(arraybases, 0, sizeof(arraybases));
	memset(arraystrides, 0, sizeof(arraystrides));
	memset(&MatrixIndexA, 0, sizeof(MatrixIndexA));
	memset(&MatrixIndexB, 0, sizeof(MatrixIndexB));
	memset(&g_VtxDesc, 0, sizeof(g_VtxDesc));
	memset(g_VtxAttr, 0, sizeof(g_VtxAttr));
	memset(texMem, 0, TMEM_SIZE);
}
