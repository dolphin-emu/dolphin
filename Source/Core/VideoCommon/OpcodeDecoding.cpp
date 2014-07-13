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

#include "Common/Common.h"
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


u8* g_pVideoData = nullptr;
bool g_bRecordFifoData = false;

typedef void (*DataReadU32xNfunc)(u32 *buf);
#if _M_SSE >= 0x301
static DataReadU32xNfunc DataReadU32xFuncs_SSSE3[16] = {
	DataReadU32xN_SSSE3<1>,
	DataReadU32xN_SSSE3<2>,
	DataReadU32xN_SSSE3<3>,
	DataReadU32xN_SSSE3<4>,
	DataReadU32xN_SSSE3<5>,
	DataReadU32xN_SSSE3<6>,
	DataReadU32xN_SSSE3<7>,
	DataReadU32xN_SSSE3<8>,
	DataReadU32xN_SSSE3<9>,
	DataReadU32xN_SSSE3<10>,
	DataReadU32xN_SSSE3<11>,
	DataReadU32xN_SSSE3<12>,
	DataReadU32xN_SSSE3<13>,
	DataReadU32xN_SSSE3<14>,
	DataReadU32xN_SSSE3<15>,
	DataReadU32xN_SSSE3<16>
};
#endif

static DataReadU32xNfunc DataReadU32xFuncs[16] = {
	DataReadU32xN<1>,
	DataReadU32xN<2>,
	DataReadU32xN<3>,
	DataReadU32xN<4>,
	DataReadU32xN<5>,
	DataReadU32xN<6>,
	DataReadU32xN<7>,
	DataReadU32xN<8>,
	DataReadU32xN<9>,
	DataReadU32xN<10>,
	DataReadU32xN<11>,
	DataReadU32xN<12>,
	DataReadU32xN<13>,
	DataReadU32xN<14>,
	DataReadU32xN<15>,
	DataReadU32xN<16>
};

static u32 Decode(bool skipped_frame);

static void InterpretDisplayList(u32 address, u32 size, bool skipped_frame)
{
	u8* old_pVideoData = g_pVideoData;
	u8* start_address = Memory::GetPointer(address);

	// Avoid the crash if Memory::GetPointer failed ..
	if (start_address != nullptr)
	{
		g_pVideoData = start_address;

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();

		u8* end = g_pVideoData + size;
		while (g_pVideoData < end)
		{
			Decode(skipped_frame);
		}
		INCSTAT(stats.thisFrame.numDListsCalled);

		// un-swap
		Statistics::SwapDL();
	}

	// reset to the old pointer
	g_pVideoData = old_pVideoData;
}

static u32 Decode(bool skipped_frame)
{
	u8* opcode_start = g_pVideoData;

	u32 buffer_size = (u32)(GetVideoBufferEndPtr() - opcode_start);
	if (buffer_size == 0)
		return 0;

	u32 cycles = 0;
	u8 cmd = DataPeek8(0);
	switch (cmd)
	{
	case GX_NOP:
		DataSkip(1); // cmd
		cycles = 6;
		break;

	case GX_LOAD_CP_REG:
		{
			if (buffer_size < 6)
				return 0;

			DataSkip(1); // cmd
			u8 sub_cmd = DataReadU8();
			u32 value = DataReadU32();
			LoadCPReg(sub_cmd, value);
			INCSTAT(stats.thisFrame.numCPLoads);
			cycles = 12;
		}
		break;

	case GX_LOAD_XF_REG:
		{
			// check if we can read the header
			if (buffer_size < 5)
				return 0;

			u32 sub_cmd = DataPeek32(1);
			int transfer_size = ((sub_cmd >> 16) & 15) + 1;
			u32 command_size = 1 + 4 + transfer_size * 4;
			if (buffer_size < command_size)
				return 0;

			DataSkip(5); // cmd, sub_cmd

			cycles = 18 + 6 * transfer_size;

			u32 xf_address = sub_cmd & 0xFFFF;
			// TODO: investigate if this intermediate buffer makes sense
			GC_ALIGNED128(u32 data_buffer[16]);
			DataReadU32xFuncs[transfer_size-1](data_buffer);
			LoadXFReg(transfer_size, xf_address, data_buffer);

			INCSTAT(stats.thisFrame.numXFLoads);
		}
		break;

	case GX_LOAD_INDX_A: // used for position matrices
	case GX_LOAD_INDX_B: // used for normal matrices
	case GX_LOAD_INDX_C: // used for postmatrices
	case GX_LOAD_INDX_D: // used for lights
		if (buffer_size < 5)
			return 0;

		DataSkip(1); // cmd
		LoadIndexedXF(DataReadU32(), 8 | (cmd >> 3));
		cycles = 6; // TODO
		break;

	case GX_CMD_CALL_DL:
		{
			if (buffer_size < 9)
				return 0;

			DataSkip(1); // cmd
			u32 address = DataReadU32();
			u32 count = DataReadU32();
			InterpretDisplayList(address, count, skipped_frame);
			cycles = 45;  // This is unverified
		}
		break;

	case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
		DataSkip(1); // cmd
		DEBUG_LOG(VIDEO, "GX 0x44: %08x", cmd);
		cycles = 6;
		break;

	case GX_CMD_INVL_VC: // Invalidate Vertex Cache - no parameters
		DataSkip(1); // cmd
		DEBUG_LOG(VIDEO, "Invalidate (vertex cache?)");
		cycles = 6;
		break;

	case GX_LOAD_BP_REG:
		{
			if (buffer_size < 5)
				return 0;

			DataSkip(1); // cmd
			u32 bp_cmd = DataReadU32();
			LoadBPReg(bp_cmd);
			INCSTAT(stats.thisFrame.numBPLoads);
			cycles = 12;
		}
		break;

	// draw primitives
	default:
		if ((cmd & 0xC0) == 0x80)
		{
			// check if we can read the header
			if (buffer_size < 3)
				return 0;

			// load vertices
			u16 vertex_count = DataPeek16(1);
			u32 vertices_size = vertex_count * VertexLoaderManager::GetVertexSize(cmd & GX_VAT_MASK);
			u32 command_size = 1 + 2 + vertices_size;
			if (buffer_size < command_size)
				return 0;

			DataSkip(3); // cmd, vertex_count

			if (skipped_frame)
			{
				DataSkip(vertices_size);
			}
			else
			{
				VertexLoaderManager::RunVertices(
					cmd & GX_VAT_MASK,   // Vertex loader index (0 - 7)
					(cmd & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
					vertex_count);
			}
			cycles = 1600; // This depends on the number of pixels rendered
		}
		else
		{
			// TODO(Omega): Maybe dump FIFO to file on this error
			std::string temp = StringFromFormat(
				"GFX FIFO: Unknown Opcode (0x%x).\n"
				"This means one of the following:\n"
				"* The emulated GPU got desynced, disabling dual core can help\n"
				"* Command stream corrupted by some spurious memory bug\n"
				"* This really is an unknown opcode (unlikely)\n"
				"* Some other sort of bug\n\n"
				"Dolphin will now likely crash or hang. Enjoy." , cmd);
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
					,cmd, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
					,fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint, fifo.bFF_GPReadEnable ? "true" : "false"
					,fifo.bFF_BPEnable ? "true" : "false" ,fifo.bFF_BPInt ? "true" : "false"
					,fifo.bFF_Breakpoint ? "true" : "false");

				Host_SysMessage(tmp.c_str());
				INFO_LOG(VIDEO, "%s", tmp.c_str());
			}
		}
	}

	// Display lists get added directly into the FIFO stream
	if (g_bRecordFifoData && cmd != GX_CMD_CALL_DL)
	{
		FifoRecorder::GetInstance().WriteGPCommand(opcode_start, u32(g_pVideoData - opcode_start));
	}

	return cycles;
}

void OpcodeDecoder_Init()
{
	g_pVideoData = GetVideoBufferStartPtr();

#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		for (int i = 0; i < 16; ++i)
			DataReadU32xFuncs[i] = DataReadU32xFuncs_SSSE3[i];
	}
#endif
}

void OpcodeDecoder_Shutdown()
{
}

u32 OpcodeDecoder_Run(bool skipped_frame)
{
	u32 total_cycles = 0;
	u32 cycles;

	do
	{
		cycles = Decode(skipped_frame);
		total_cycles += cycles;
	} while (cycles);

	return total_cycles;
}
