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

#ifndef _FIFORECORDANALYZER_H_
#define _FIFORECORDANALYZER_H_

#include "FifoAnalyzer.h"

#include "Common.h"

#include "BPMemory.h"

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
	void ProcessLoadIndexedXf(u32 val, int array);
	void ProcessVertexArrays(u8 *data, u8 vtxAttrGroup);
	void ProcessTexMaps();

	void WriteVertexArray(int arrayIndex, u8 *vertexData, int vertexSize, int numVertices);
	void WriteTexMapMemory(int texMap, u32 &writtenTexMaps);

	bool m_DrawingObject;

	BPMemory *m_BpMem;
	FifoAnalyzer::CPMemory m_CpMem;	
};

#endif