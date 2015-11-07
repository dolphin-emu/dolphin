// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Assert.h"
#include "Core/Core.h"
#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoRecordAnalyzer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/TextureDecoder.h"

using namespace FifoAnalyzer;

static bool s_DrawingObject;
FifoAnalyzer::CPMemory s_CpMem;

static void DecodeOpcode(u8* data);
static void ProcessLoadIndexedXf(u32 val, int array);
static void ProcessVertexArrays(u8* data, u8 vtxAttrGroup);
static void WriteVertexArray(int arrayIndex, u8* vertexData, int vertexSize, int numVertices);

void FifoRecordAnalyzer::Initialize(u32* cpMem)
{
	s_DrawingObject = false;

	FifoAnalyzer::LoadCPReg(0x50, *(cpMem + 0x50), s_CpMem);
	FifoAnalyzer::LoadCPReg(0x60, *(cpMem + 0x60), s_CpMem);
	for (int i = 0; i < 8; ++i)
		FifoAnalyzer::LoadCPReg(0x70 + i, *(cpMem + 0x70 + i), s_CpMem);

	memcpy(s_CpMem.arrayBases, cpMem + 0xA0, 16 * 4);
	memcpy(s_CpMem.arrayStrides, cpMem + 0xB0, 16 * 4);
}

void FifoRecordAnalyzer::AnalyzeGPCommand(u8* data)
{
	DecodeOpcode(data);
}

static void DecodeOpcode(u8* data)
{
	int cmd = ReadFifo8(data);

	switch (cmd)
	{
	case GX_NOP:
	case 0x44:
	case GX_CMD_INVL_VC:
		break;

	case GX_LOAD_CP_REG:
		{
			s_DrawingObject = false;

			u32 cmd2 = ReadFifo8(data);
			u32 value = ReadFifo32(data);
			FifoAnalyzer::LoadCPReg(cmd2, value, s_CpMem);
		}

		break;

	case GX_LOAD_XF_REG:
		s_DrawingObject = false;
		break;

	case GX_LOAD_INDX_A:
		s_DrawingObject = false;
		ProcessLoadIndexedXf(ReadFifo32(data), 0xc);
		break;
	case GX_LOAD_INDX_B:
		s_DrawingObject = false;
		ProcessLoadIndexedXf(ReadFifo32(data), 0xd);
		break;
	case GX_LOAD_INDX_C:
		s_DrawingObject = false;
		ProcessLoadIndexedXf(ReadFifo32(data), 0xe);
		break;
	case GX_LOAD_INDX_D:
		s_DrawingObject = false;
		ProcessLoadIndexedXf(ReadFifo32(data), 0xf);
		break;

	case GX_CMD_CALL_DL:
		{
			// The recorder should have expanded display lists into the fifo stream and skipped the call to start them
			// That is done to make it easier to track where memory is updated
			_assert_(false);
		}
		break;

	case GX_LOAD_BP_REG:
		s_DrawingObject = false;
		ReadFifo32(data);
		break;

	default:
		if (cmd & 0x80)
		{
			if (!s_DrawingObject)
			{
				s_DrawingObject = true;
			}

			ProcessVertexArrays(data, cmd & GX_VAT_MASK);
		}
		else
		{
			PanicAlert("FifoRecordAnalyzer: Unknown Opcode (0x%x).\n", cmd);
		}
	}
}

static void ProcessLoadIndexedXf(u32 val, int array)
{
	int index = val >> 16;
	int size = ((val >> 12) & 0xF) + 1;

	u32 address = s_CpMem.arrayBases[array] + s_CpMem.arrayStrides[array] * index;

	FifoRecorder::GetInstance().UseMemory(address, size * 4, MemoryUpdate::XF_DATA);
}

static void ProcessVertexArrays(u8* data, u8 vtxAttrGroup)
{
	int sizes[21];
	FifoAnalyzer::CalculateVertexElementSizes(sizes, vtxAttrGroup, s_CpMem);

	// Determine offset of each element from start of vertex data
	int offsets[12];
	int offset = 0;
	for (int i = 0; i < 12; ++i)
	{
		offsets[i] = offset;
		offset += sizes[i + 9];
	}

	int vertexSize = offset;
	int numVertices = ReadFifo16(data);

	if (numVertices > 0)
	{
		for (int i = 0; i < 12; ++i)
		{
			WriteVertexArray(i, data + offsets[i], vertexSize, numVertices);
		}
	}
}

static void WriteVertexArray(int arrayIndex, u8* vertexData, int vertexSize, int numVertices)
{
	// Skip if not indexed array
	int arrayType = (s_CpMem.vtxDesc.Hex >> (9 + (arrayIndex * 2))) & 3;
	if (arrayType < 2)
		return;

	int maxIndex = 0;

	// Determine min and max indices
	if (arrayType == INDEX8)
	{
		for (int i = 0; i < numVertices; ++i)
		{
			int index = *vertexData;
			vertexData += vertexSize;

			// 0xff skips the vertex
			if (index != 0xff)
			{
				if (index > maxIndex)
					maxIndex = index;
			}
		}
	}
	else
	{
		for (int i = 0; i < numVertices; ++i)
		{
			int index = Common::swap16(vertexData);
			vertexData += vertexSize;

			// 0xffff skips the vertex
			if (index != 0xffff)
			{
				if (index > maxIndex)
					maxIndex = index;
			}
		}
	}

	u32 arrayStart = s_CpMem.arrayBases[arrayIndex];
	u32 arraySize = s_CpMem.arrayStrides[arrayIndex] * (maxIndex + 1);

	FifoRecorder::GetInstance().UseMemory(arrayStart, arraySize, MemoryUpdate::VERTEX_STREAM);
}
