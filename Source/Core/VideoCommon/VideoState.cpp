// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoState.h"
#include "VideoCommon/XFMemory.h"

static void DoState(PointerWrap &p)
{
	// BP Memory
	p.Do(bpmem);
	p.DoMarker("BP Memory");

	// CP Memory
	DoCPState(p);

	// XF Memory
	p.Do(xfmem);
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
	memset(&g_main_cp_state, 0, sizeof(g_main_cp_state));
	memset(&g_preprocess_cp_state, 0, sizeof(g_preprocess_cp_state));
	memset(texMem, 0, TMEM_SIZE);
}
