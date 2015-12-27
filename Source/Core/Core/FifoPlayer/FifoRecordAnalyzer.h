// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "Core/FifoPlayer/FifoAnalyzer.h"

#include "VideoCommon/BPMemory.h"

namespace FifoRecordAnalyzer
{
	// Must call this before analyzing Fifo commands with FifoAnalyzer::AnalyzeCommand()
	void Initialize(u32* cpMem);

	void ProcessLoadIndexedXf(u32 val, int array);
	void WriteVertexArray(int arrayIndex, u8* vertexData, int vertexSize, int numVertices);
};
