// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "VideoBackends/Software/BPMemLoader.h"
#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoBackends/Software/DebugUtil.h"
#include "VideoBackends/Software/OpcodeDecoder.h"
#include "VideoBackends/Software/SWCommandProcessor.h"
#include "VideoBackends/Software/SWStatistics.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/SWVideoConfig.h"
#include "VideoBackends/Software/XFMemLoader.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"

typedef void (*DecodingFunction)(u32);

namespace OpcodeDecoder
{
static DecodingFunction currentFunction = nullptr;
static u32 minCommandSize;
static u16 streamSize;
static u16 streamAddress;
static bool readOpcode;
static SWVertexLoader vertexLoader;
static bool inObjectStream;
static u8 lastPrimCmd;


void DoState(PointerWrap &p)
{
	p.Do(minCommandSize);
	// Not sure what is wrong with this. Something(s) in here is causing dolphin to crash/hang when loading states saved from another run of dolphin. Doesn't seem too important anyway...
	//vertexLoader.DoState(p);
	p.Do(readOpcode);
	p.Do(inObjectStream);
	p.Do(lastPrimCmd);
	p.Do(streamSize);
	p.Do(streamAddress);
	if (p.GetMode() == PointerWrap::MODE_READ)
		  ResetDecoding();
}

static void DecodePrimitiveStream(u32 iBufferSize)
{
	u32 vertexSize = vertexLoader.GetVertexSize();

	bool skipPrimitives = g_bSkipCurrentFrame ||
		 swstats.thisFrame.numDrawnObjects < g_SWVideoConfig.drawStart ||
		 swstats.thisFrame.numDrawnObjects >= g_SWVideoConfig.drawEnd;

	if (skipPrimitives)
	{
		while (streamSize > 0 && iBufferSize >= vertexSize)
		{
			g_video_buffer_read_ptr += vertexSize;
			iBufferSize -= vertexSize;
			streamSize--;
		}
	}
	else
	{
		while (streamSize > 0 && iBufferSize >= vertexSize)
		{
			vertexLoader.LoadVertex();
			iBufferSize -= vertexSize;
			streamSize--;
		}
	}

	if (streamSize == 0)
	{
		// return to normal command processing
		ResetDecoding();
	}
}

static void ReadXFData(u32 iBufferSize)
{
	_assert_msg_(VIDEO, iBufferSize >= (u32)(streamSize * 4), "Underflow during standard opcode decoding");

	u32 pData[16];
	for (int i = 0; i < streamSize; i++)
		pData[i] = DataReadU32();
	SWLoadXFReg(streamSize, streamAddress, pData);

	// return to normal command processing
	ResetDecoding();
}

static void ExecuteDisplayList(u32 addr, u32 count)
{
	u8 *videoDataSave = g_video_buffer_read_ptr;

	u8 *dlStart = Memory::GetPointer(addr);

	g_video_buffer_read_ptr = dlStart;

	while (OpcodeDecoder::CommandRunnable(count))
	{
		OpcodeDecoder::Run(count);

		// if data was read by the opcode decoder then the video data pointer changed
		u32 readCount = (u32)(g_video_buffer_read_ptr - dlStart);
		dlStart = g_video_buffer_read_ptr;

		_assert_msg_(VIDEO, count >= readCount, "Display list underrun");

		count -= readCount;
	}

	g_video_buffer_read_ptr = videoDataSave;
}

static void DecodeStandard(u32 bufferSize)
{
	_assert_msg_(VIDEO, CommandRunnable(bufferSize), "Underflow during standard opcode decoding");

	int Cmd = DataReadU8();

	if (Cmd == GX_NOP)
		return;
	// Causes a SIGBUS error on Android
	// XXX: Investigate
#ifndef ANDROID
	// check if switching in or out of an object
	// only used for debugging
	if (inObjectStream && (Cmd & 0x87) != lastPrimCmd)
	{
		inObjectStream = false;
		DebugUtil::OnObjectEnd();
	}
	if (Cmd & 0x80 && !inObjectStream)
	{
		inObjectStream = true;
		lastPrimCmd = Cmd & 0x87;
		DebugUtil::OnObjectBegin();
	}
#endif
	switch (Cmd)
	{
	case GX_NOP:
		break;

	case GX_LOAD_CP_REG: //0x08
		{
			u32 SubCmd = DataReadU8();
			u32 Value = DataReadU32();
			SWLoadCPReg(SubCmd, Value);
		}
		break;

	case GX_LOAD_XF_REG:
		{
			u32 Cmd2 = DataReadU32();
			streamSize = ((Cmd2 >> 16) & 15) + 1;
			streamAddress = Cmd2 & 0xFFFF;
			currentFunction = ReadXFData;
			minCommandSize = streamSize * 4;
			readOpcode = false;
		}
		break;

	case GX_LOAD_INDX_A: //used for position matrices
		SWLoadIndexedXF(DataReadU32(), 0xC);
		break;
	case GX_LOAD_INDX_B: //used for normal matrices
		SWLoadIndexedXF(DataReadU32(), 0xD);
		break;
	case GX_LOAD_INDX_C: //used for postmatrices
		SWLoadIndexedXF(DataReadU32(), 0xE);
		break;
	case GX_LOAD_INDX_D: //used for lights
		SWLoadIndexedXF(DataReadU32(), 0xF);
		break;

	case GX_CMD_CALL_DL:
		{
			u32 dwAddr  = DataReadU32();
			u32 dwCount = DataReadU32();
			ExecuteDisplayList(dwAddr, dwCount);
		}
		break;

	case 0x44:
		// zelda 4 swords calls it and checks the metrics registers after that
		break;

	case GX_CMD_INVL_VC:// Invalidate (vertex cache?)
		DEBUG_LOG(VIDEO, "Invalidate  (vertex cache?)");
		break;

	case GX_LOAD_BP_REG: //0x61
		{
			u32 cmd = DataReadU32();
			SWLoadBPReg(cmd);
		}
		break;

	// draw primitives
	default:
		if ((Cmd & 0xC0) == 0x80)
		{
			u8 vatIndex = Cmd & GX_VAT_MASK;
			u8 primitiveType = (Cmd & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT;
			vertexLoader.SetFormat(vatIndex, primitiveType);

			// switch to primitive processing
			streamSize = DataReadU16();
			currentFunction = DecodePrimitiveStream;
			minCommandSize = vertexLoader.GetVertexSize();
			readOpcode = false;

			INCSTAT(swstats.thisFrame.numPrimatives);
			DEBUG_LOG(VIDEO, "Draw begin");
		}
		else
		{
			PanicAlert("GFX: Unknown Opcode (0x%x).\n", Cmd);
			break;
		}
		break;
	}
}


void Init()
{
	inObjectStream = false;
	lastPrimCmd = 0;
	ResetDecoding();
}

void ResetDecoding()
{
	currentFunction = DecodeStandard;
	minCommandSize = 1;
	readOpcode = true;
}

bool CommandRunnable(u32 iBufferSize)
{
	if (iBufferSize < minCommandSize)
		return false;

	if (readOpcode)
	{
		u8 Cmd = DataPeek8(0);
		u32 minSize = 1;

		switch (Cmd)
		{
		case GX_LOAD_CP_REG: //0x08
			minSize = 6;
			break;

		case GX_LOAD_XF_REG:
			minSize = 5;
			break;

		case GX_LOAD_INDX_A: //used for position matrices
			minSize = 5;
			break;
		case GX_LOAD_INDX_B: //used for normal matrices
			minSize = 5;
			break;
		case GX_LOAD_INDX_C: //used for postmatrices
			minSize = 5;
			break;
		case GX_LOAD_INDX_D: //used for lights
			minSize = 5;
			break;

		case GX_CMD_CALL_DL:
			minSize = 9;
			break;

		case GX_LOAD_BP_REG: //0x61
			minSize = 5;
			break;

		// draw primitives
		default:
			if ((Cmd & 0xC0) == 0x80)
				minSize = 3;
			break;
		}

		return (iBufferSize >= minSize);
	}

	return true;
}

void Run(u32 iBufferSize)
{
	currentFunction(iBufferSize);
}

}
