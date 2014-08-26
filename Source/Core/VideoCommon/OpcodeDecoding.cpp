// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

//DL facts:
//  Ikaruga uses (nearly) NO display lists!
//  Zelda WW uses TONS of display lists
//  Zelda TP uses almost 100% display lists except menus (we like this!)
//  Super Mario Galaxy has nearly all geometry and more than half of the state in DLs (great!)

// Note that it IS NOT GENERALLY POSSIBLE to precompile display lists! You can compile them as they are
// while interpreting them, and hope that the vertex format doesn't change, though, if you do it right
// when they are called. The reason is that the vertex format affects the sizes of the vertices.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"


bool g_bRecordFifoData = false;

static u32 InterpretDisplayList(u32 address, u32 size)
{
	u8* old_pVideoData = g_video_buffer_read_ptr;
	u8* startAddress = Memory::GetPointer(address);

	u32 cycles = 0;

	// Avoid the crash if Memory::GetPointer failed ..
	if (startAddress != nullptr)
	{
		g_video_buffer_read_ptr = startAddress;

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();

		u8 *end = g_video_buffer_read_ptr + size;
		cycles = OpcodeDecoder_Run(end);
		INCSTAT(stats.thisFrame.numDListsCalled);

		// un-swap
		Statistics::SwapDL();
	}

	// reset to the old pointer
	g_video_buffer_read_ptr = old_pVideoData;

	return cycles;
}

static void UnknownOpcode(u8 cmd_byte, void *buffer, bool preprocess)
{
	// TODO(Omega): Maybe dump FIFO to file on this error
	std::string temp = StringFromFormat(
		"GFX FIFO: Unknown Opcode (0x%x @ %p).\n"
		"This means one of the following:\n"
		"* The emulated GPU got desynced, disabling dual core can help\n"
		"* Command stream corrupted by some spurious memory bug\n"
		"* This really is an unknown opcode (unlikely)\n"
		"* Some other sort of bug\n\n"
		"Dolphin will now likely crash or hang. Enjoy." ,
		cmd_byte,
		buffer);
	Host_SysMessage(temp.c_str());
	INFO_LOG(VIDEO, "%s", temp.c_str());
	{
		SCPFifoStruct &fifo = CommandProcessor::fifo;

		std::string tmp = StringFromFormat(
			"Illegal command %02x\n"
			"CPBase: 0x%08x\n"
			"CPEnd: 0x%08x\n"
			"CPHiWatermark: 0x%08x\n"
			"CPLoWatermark: 0x%08x\n"
			"CPReadWriteDistance: 0x%08x\n"
			"CPWritePointer: 0x%08x\n"
			"CPReadPointer: 0x%08x\n"
			"CPBreakpoint: 0x%08x\n"
			"bFF_GPReadEnable: %s\n"
			"bFF_BPEnable: %s\n"
			"bFF_BPInt: %s\n"
			"bFF_Breakpoint: %s\n"
			,cmd_byte, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
			,fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint, fifo.bFF_GPReadEnable ? "true" : "false"
			,fifo.bFF_BPEnable ? "true" : "false" ,fifo.bFF_BPInt ? "true" : "false"
			,fifo.bFF_Breakpoint ? "true" : "false");

		Host_SysMessage(tmp.c_str());
		INFO_LOG(VIDEO, "%s", tmp.c_str());
	}
}

static u32 Decode(u8* end)
{
	u8 *opcodeStart = g_video_buffer_read_ptr;
	if (g_video_buffer_read_ptr == end)
		return 0;

	u8 cmd_byte = DataReadU8();
	u32 cycles;
	switch (cmd_byte)
	{
	case GX_NOP:
		cycles = 6; // Hm, this means that we scan over nop streams pretty slowly...
		break;

	case GX_LOAD_CP_REG: //0x08
		{
			if (end - g_video_buffer_read_ptr < 1 + 4)
				return 0;
			cycles = 12;
			u8 sub_cmd = DataReadU8();
			u32 value = DataReadU32();
			LoadCPReg(sub_cmd, value);
			INCSTAT(stats.thisFrame.numCPLoads);
		}
		break;

	case GX_LOAD_XF_REG:
		{
			if (end - g_video_buffer_read_ptr < 4)
				return 0;
			u32 Cmd2 = DataReadU32();
			int transfer_size = ((Cmd2 >> 16) & 15) + 1;
			if ((size_t) (end - g_video_buffer_read_ptr) < transfer_size * sizeof(u32))
				return 0;
			cycles = 18 + 6 * transfer_size;
			u32 xf_address = Cmd2 & 0xFFFF;
			LoadXFReg(transfer_size, xf_address);

			INCSTAT(stats.thisFrame.numXFLoads);
		}
		break;

	case GX_LOAD_INDX_A: //used for position matrices
		if (end - g_video_buffer_read_ptr < 4)
			return 0;
		cycles = 6;
		LoadIndexedXF(DataReadU32(), 0xC);
		break;
	case GX_LOAD_INDX_B: //used for normal matrices
		if (end - g_video_buffer_read_ptr < 4)
			return 0;
		cycles = 6;
		LoadIndexedXF(DataReadU32(), 0xD);
		break;
	case GX_LOAD_INDX_C: //used for postmatrices
		if (end - g_video_buffer_read_ptr < 4)
			return 0;
		cycles = 6;
		LoadIndexedXF(DataReadU32(), 0xE);
		break;
	case GX_LOAD_INDX_D: //used for lights
		if (end - g_video_buffer_read_ptr < 4)
			return 0;
		cycles = 6;
		LoadIndexedXF(DataReadU32(), 0xF);
		break;

	case GX_CMD_CALL_DL:
		{
			if (end - g_video_buffer_read_ptr < 8)
				return 0;
			u32 address = DataReadU32();
			u32 count = DataReadU32();
			cycles = 6 + InterpretDisplayList(address, count);
		}
		break;

	case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
		cycles = 6;
		DEBUG_LOG(VIDEO, "GX 0x44: %08x", cmd_byte);
		break;

	case GX_CMD_INVL_VC: // Invalidate Vertex Cache
		cycles = 6;
		DEBUG_LOG(VIDEO, "Invalidate (vertex cache?)");
		break;

	case GX_LOAD_BP_REG: //0x61
		// In skipped_frame case: We have to let BP writes through because they set
		// tokens and stuff.  TODO: Call a much simplified LoadBPReg instead.
		{
			if (end - g_video_buffer_read_ptr < 4)
				return 0;
			cycles = 12;
			u32 bp_cmd = DataReadU32();
			LoadBPReg(bp_cmd);
			INCSTAT(stats.thisFrame.numBPLoads);
		}
		break;

	// draw primitives
	default:
		if ((cmd_byte & 0xC0) == 0x80)
		{
			cycles = 1600;
			// load vertices
			if (end - g_video_buffer_read_ptr < 2)
				return 0;
			u16 numVertices = DataReadU16();

			if (!VertexLoaderManager::RunVertices(
				cmd_byte & GX_VAT_MASK,   // Vertex loader index (0 - 7)
				(cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
				numVertices,
				end - g_video_buffer_read_ptr,
				g_bSkipCurrentFrame))
			{
				return 0;
			}
		}
		else
		{
			UnknownOpcode(cmd_byte, opcodeStart, false);
			cycles = 1;
		}
		break;
	}

	// Display lists get added directly into the FIFO stream
	if (g_bRecordFifoData && cmd_byte != GX_CMD_CALL_DL)
		FifoRecorder::GetInstance().WriteGPCommand(opcodeStart, u32(g_video_buffer_read_ptr - opcodeStart));

	return cycles;
}

void OpcodeDecoder_Init()
{
	g_video_buffer_read_ptr = GetVideoBufferStartPtr();
}


void OpcodeDecoder_Shutdown()
{
}

u32 OpcodeDecoder_Run(u8* end)
{
	u32 totalCycles = 0;
	while (true)
	{
		u8* old = g_video_buffer_read_ptr;
		u32 cycles = Decode(end);
		if (cycles == 0)
		{
			g_video_buffer_read_ptr = old;
			break;
		}
		totalCycles += cycles;
	}
	return totalCycles;
}
