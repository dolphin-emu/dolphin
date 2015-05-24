// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "Core/FifoPlayer/FifoAnalyzer.h"

#include "VideoCommon/BPMemory.h"

class FifoRecordAnalyzer
{
public:
	FifoRecordAnalyzer();

	// Must call this before analyzing GP commands
	void Initialize(u32 *bpMem, u32 *cpMem);

	// Assumes data contains all information for the command
	// Calls FifoRecorder::WriteMemory
	void AnalyzeGPCommand(u8 *data);

private:
	void DecodeOpcode(u8 *data);

	void ProcessLoadTlut1();
	void ProcessPreloadTexture();
	void ProcessLoadIndexedXf(u32 val, int array);
	void ProcessVertexArrays(u8 *data, u8 vtxAttrGroup);
	void ProcessTexMaps();

	void WriteVertexArray(int arrayIndex, u8 *vertexData, int vertexSize, int numVertices);
	void WriteTexMapMemory(int texMap, u32 &writtenTexMaps);

	bool m_DrawingObject;

	BPMemory *m_BpMem;
	FifoAnalyzer::CPMemory m_CpMem;
};
