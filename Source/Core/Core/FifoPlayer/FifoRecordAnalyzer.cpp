// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Core/Core.h"
#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoRecordAnalyzer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/TextureDecoder.h"

using namespace FifoAnalyzer;


FifoRecordAnalyzer::FifoRecordAnalyzer() :
	m_DrawingObject(false),
	m_BpMem(nullptr)
{
}

void FifoRecordAnalyzer::Initialize(u32 *bpMem, u32 *cpMem)
{
	m_DrawingObject = false;

	m_BpMem = (BPMemory*)bpMem;

	FifoAnalyzer::LoadCPReg(0x50, *(cpMem + 0x50), m_CpMem);
	FifoAnalyzer::LoadCPReg(0x60, *(cpMem + 0x60), m_CpMem);
	for (int i = 0; i < 8; ++i)
		FifoAnalyzer::LoadCPReg(0x70 + i, *(cpMem + 0x70 + i), m_CpMem);

	memcpy(m_CpMem.arrayBases, cpMem + 0xA0, 16 * 4);
	memcpy(m_CpMem.arrayStrides, cpMem + 0xB0, 16 * 4);
}

void FifoRecordAnalyzer::AnalyzeGPCommand(u8 *data)
{
	DecodeOpcode(data);
}

void FifoRecordAnalyzer::DecodeOpcode(u8 *data)
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
			m_DrawingObject = false;

			u32 cmd2 = ReadFifo8(data);
			u32 value = ReadFifo32(data);
			FifoAnalyzer::LoadCPReg(cmd2, value, m_CpMem);
		}

		break;

	case GX_LOAD_XF_REG:
		m_DrawingObject = false;
		break;

	case GX_LOAD_INDX_A:
		m_DrawingObject = false;
		ProcessLoadIndexedXf(ReadFifo32(data), 0xc);
		break;
	case GX_LOAD_INDX_B:
		m_DrawingObject = false;
		ProcessLoadIndexedXf(ReadFifo32(data), 0xd);
		break;
	case GX_LOAD_INDX_C:
		m_DrawingObject = false;
		ProcessLoadIndexedXf(ReadFifo32(data), 0xe);
		break;
	case GX_LOAD_INDX_D:
		m_DrawingObject = false;
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
		{
			m_DrawingObject = false;

			u32 cmd2 = ReadFifo32(data);
			BPCmd bp = FifoAnalyzer::DecodeBPCmd(cmd2, *m_BpMem);

			if (bp.address == BPMEM_LOADTLUT1)
				ProcessLoadTlut1();
			if (bp.address == BPMEM_PRELOAD_MODE)
				ProcessPreloadTexture();
		}
		break;

	default:
		if (cmd & 0x80)
		{
			if (!m_DrawingObject)
			{
				m_DrawingObject = true;
				ProcessTexMaps();
			}

			ProcessVertexArrays(data, cmd & GX_VAT_MASK);
		}
		else
		{
			PanicAlert("FifoRecordAnalyzer: Unknown Opcode (0x%x).\n", cmd);
		}
	}
}

void FifoRecordAnalyzer::ProcessLoadTlut1()
{
	u32 tlutXferCount;
	u32 tlutMemAddr;
	u32 memAddr;

	GetTlutLoadData(tlutMemAddr, memAddr, tlutXferCount, *m_BpMem);

	FifoRecorder::GetInstance().WriteMemory(memAddr, tlutXferCount, MemoryUpdate::TMEM);
}

void FifoRecordAnalyzer::ProcessPreloadTexture()
{
	BPS_TmemConfig& tmem_cfg = m_BpMem->tmem_config;
	//u32 tmem_addr = tmem_cfg.preload_tmem_even * TMEM_LINE_SIZE;
	u32 size = tmem_cfg.preload_tile_info.count * TMEM_LINE_SIZE; // TODO: Should this be half size for RGBA8 preloads?

	FifoRecorder::GetInstance().WriteMemory(tmem_cfg.preload_addr << 5, size, MemoryUpdate::TMEM);
}

void FifoRecordAnalyzer::ProcessLoadIndexedXf(u32 val, int array)
{
	int index = val >> 16;
	int size = ((val >> 12) & 0xF) + 1;

	u32 address = m_CpMem.arrayBases[array] + m_CpMem.arrayStrides[array] * index;

	FifoRecorder::GetInstance().WriteMemory(address, size * 4, MemoryUpdate::XF_DATA);
}

void FifoRecordAnalyzer::ProcessVertexArrays(u8 *data, u8 vtxAttrGroup)
{
	int sizes[21];
	FifoAnalyzer::CalculateVertexElementSizes(sizes, vtxAttrGroup, m_CpMem);

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

void FifoRecordAnalyzer::WriteVertexArray(int arrayIndex, u8 *vertexData, int vertexSize, int numVertices)
{
	// Skip if not indexed array
	int arrayType = (m_CpMem.vtxDesc.Hex >> (9 + (arrayIndex * 2))) & 3;
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

	u32 arrayStart = m_CpMem.arrayBases[arrayIndex];
	u32 arraySize = m_CpMem.arrayStrides[arrayIndex] * (maxIndex + 1);

	FifoRecorder::GetInstance().WriteMemory(arrayStart, arraySize, MemoryUpdate::VERTEX_STREAM);
}

void FifoRecordAnalyzer::ProcessTexMaps()
{
	u32 writtenTexMaps = 0;

	// Texture maps used in TEV indirect stages
	for (u32 i = 0; i < m_BpMem->genMode.numindstages; ++i)
	{
		u32 texMap = m_BpMem->tevindref.getTexMap(i);

		WriteTexMapMemory(texMap, writtenTexMaps);
	}

	// Texture maps used in TEV direct stages
	for (u32 i = 0; i <= m_BpMem->genMode.numtevstages; ++i)
	{
		int stageNum2 = i >> 1;
		int stageOdd = i & 1;
		TwoTevStageOrders &order = m_BpMem->tevorders[stageNum2];
		int texMap = order.getTexMap(stageOdd);

		if (order.getEnable(stageOdd))
			WriteTexMapMemory(texMap, writtenTexMaps);
	}
}

void FifoRecordAnalyzer::WriteTexMapMemory(int texMap, u32 &writtenTexMaps)
{
	// Avoid rechecking the same texture map
	u32 texMapMask = 1 << texMap;
	if (writtenTexMaps & texMapMask)
		return;

	writtenTexMaps |= texMapMask;

	FourTexUnits& texUnit = m_BpMem->tex[(texMap >> 2) & 1];
	u8 subTexmap = texMap & 3;

	TexImage0& ti0 = texUnit.texImage0[subTexmap];

	u32 width = ti0.width + 1;
	u32 height = ti0.height + 1;
	u32 imageBase = texUnit.texImage3[subTexmap].image_base << 5;

	u32 fmtWidth = TexDecoder_GetBlockWidthInTexels(ti0.format) - 1;
	u32 fmtHeight = TexDecoder_GetBlockHeightInTexels(ti0.format) - 1;
	int fmtDepth = TexDecoder_GetTexelSizeInNibbles(ti0.format);

	// Round width and height up to the next block
	width = (width + fmtWidth) & (~fmtWidth);
	height = (height + fmtHeight) & (~fmtHeight);

	u32 textureSize = (width * height * fmtDepth) / 2;

	// TODO: mip maps
	int mip = texUnit.texMode1[subTexmap].max_lod;
	if ((texUnit.texMode0[subTexmap].min_filter & 3) == 0)
		mip = 0;

	while (mip)
	{
		width >>= 1;
		height >>= 1;

		width = std::max(width, fmtWidth);
		height = std::max(height, fmtHeight);
		u32 size = (width * height * fmtDepth) >> 1;

		textureSize += size;

		mip--;
	}

	FifoRecorder::GetInstance().WriteMemory(imageBase, textureSize, MemoryUpdate::TEXTURE_MAP);
}
