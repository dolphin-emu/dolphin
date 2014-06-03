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

#if _M_SSE >= 0x301
DataReadU32xNfunc DataReadU32xFuncs_SSSE3[16] = {
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

DataReadU32xNfunc DataReadU32xFuncs[16] = {
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

extern u8* GetVideoBufferStartPtr();
extern u8* GetVideoBufferEndPtr();

static void Decode();

void InterpretDisplayList(u32 address, u32 size)
{
	u8* old_pVideoData = g_pVideoData;
	u8* startAddress = Memory::GetPointer(address);

	// Avoid the crash if Memory::GetPointer failed ..
	if (startAddress != nullptr)
	{
		g_pVideoData = startAddress;

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();

		u8 *end = g_pVideoData + size;
		while (g_pVideoData < end)
		{
			Decode();
		}
		INCSTAT(stats.numDListsCalled);
		INCSTAT(stats.thisFrame.numDListsCalled);

		// un-swap
		Statistics::SwapDL();
	}

	// reset to the old pointer
	g_pVideoData = old_pVideoData;
}

u32 FifoCommandRunnable(u32 &command_size)
{
	u32 cycleTime = 0;
	u32 buffer_size = (u32)(GetVideoBufferEndPtr() - g_pVideoData);
	if (buffer_size == 0)
		return 0;  // can't peek

	u8 cmd_byte = DataPeek8(0);

	switch (cmd_byte)
	{
	case GX_NOP: // Hm, this means that we scan over nop streams pretty slowly...
		command_size = 1;
		cycleTime = 6;
		break;
	case GX_CMD_INVL_VC: // Invalidate Vertex Cache - no parameters
		command_size = 1;
		cycleTime = 6;
		break;
	case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
		command_size = 1;
		cycleTime = 6;
		break;

	case GX_LOAD_BP_REG:
		command_size = 5;
		cycleTime = 12;
		break;

	case GX_LOAD_CP_REG:
		command_size = 6;
		cycleTime = 12;
		break;

	case GX_LOAD_INDX_A:
	case GX_LOAD_INDX_B:
	case GX_LOAD_INDX_C:
	case GX_LOAD_INDX_D:
		command_size = 5;
		cycleTime = 6; // TODO
		break;

	case GX_CMD_CALL_DL:
		{
			// FIXME: Calculate the cycle time of the display list.
			//u32 address = DataPeek32(1);
			//u32 size = DataPeek32(5);
			//u8* old_pVideoData = g_pVideoData;
			//u8* startAddress = Memory::GetPointer(address);

			//// Avoid the crash if Memory::GetPointer failed ..
			//if (startAddress != 0)
			//{
			//	g_pVideoData = startAddress;
			//	u8 *end = g_pVideoData + size;
			//	u32 step = 0;
			//	while (g_pVideoData < end)
			//	{
			//		cycleTime += FifoCommandRunnable(step);
			//		g_pVideoData += step;
			//	}
			//}
			//else
			//{
			//	cycleTime = 45;
			//}

			//// reset to the old pointer
			//g_pVideoData = old_pVideoData;
			command_size = 9;
			cycleTime = 45;  // This is unverified
		}
		break;

	case GX_LOAD_XF_REG:
		{
			// check if we can read the header
			if (buffer_size >= 5)
			{
				command_size = 1 + 4;
				u32 Cmd2 = DataPeek32(1);
				int transfer_size = ((Cmd2 >> 16) & 15) + 1;
				command_size += transfer_size * 4;
				cycleTime = 18 + 6 * transfer_size;
			}
			else
			{
				return 0;
			}
		}
		break;

	default:
		if ((cmd_byte & 0xC0) == 0x80)
		{
			// check if we can read the header
			if (buffer_size >= 3)
			{
				command_size = 1 + 2;
				u16 numVertices = DataPeek16(1);
				command_size += numVertices * VertexLoaderManager::GetVertexSize(cmd_byte & GX_VAT_MASK);
				cycleTime = 1600; // This depends on the number of pixels rendered
			}
			else
			{
				return 0;
			}
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
				"Dolphin will now likely crash or hang. Enjoy." , cmd_byte);
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
		break;
	}

	if (command_size > buffer_size)
		return 0;

	// INFO_LOG("OP detected: cmd_byte 0x%x  size %i  buffer %i",cmd_byte, command_size, buffer_size);
	if (cycleTime == 0)
		cycleTime = 6;

	return cycleTime;
}

u32 FifoCommandRunnable()
{
	u32 command_size = 0;
	return FifoCommandRunnable(command_size);
}

static void Decode()
{
	u8 *opcodeStart = g_pVideoData;

	int cmd_byte = DataReadU8();
	switch (cmd_byte)
	{
	case GX_NOP:
		break;

	case GX_LOAD_CP_REG: //0x08
		{
			u8 sub_cmd = DataReadU8();
			u32 value = DataReadU32();
			LoadCPReg(sub_cmd, value);
			INCSTAT(stats.thisFrame.numCPLoads);
		}
		break;

	case GX_LOAD_XF_REG:
		{
			u32 Cmd2 = DataReadU32();
			int transfer_size = ((Cmd2 >> 16) & 15) + 1;
			u32 xf_address = Cmd2 & 0xFFFF;
			GC_ALIGNED128(u32 data_buffer[16]);
			DataReadU32xFuncs[transfer_size-1](data_buffer);
			LoadXFReg(transfer_size, xf_address, data_buffer);

			INCSTAT(stats.thisFrame.numXFLoads);
		}
		break;

	case GX_LOAD_INDX_A: //used for position matrices
		LoadIndexedXF(DataReadU32(), 0xC);
		break;
	case GX_LOAD_INDX_B: //used for normal matrices
		LoadIndexedXF(DataReadU32(), 0xD);
		break;
	case GX_LOAD_INDX_C: //used for postmatrices
		LoadIndexedXF(DataReadU32(), 0xE);
		break;
	case GX_LOAD_INDX_D: //used for lights
		LoadIndexedXF(DataReadU32(), 0xF);
		break;

	case GX_CMD_CALL_DL:
		{
			u32 address = DataReadU32();
			u32 count = DataReadU32();
			InterpretDisplayList(address, count);
		}
		break;

	case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
		DEBUG_LOG(VIDEO, "GX 0x44: %08x", cmd_byte);
		break;

	case GX_CMD_INVL_VC: // Invalidate Vertex Cache
		DEBUG_LOG(VIDEO, "Invalidate (vertex cache?)");
		break;

	case GX_LOAD_BP_REG: //0x61
		{
			u32 bp_cmd = DataReadU32();
			LoadBPReg(bp_cmd);
			INCSTAT(stats.thisFrame.numBPLoads);
		}
		break;

	// draw primitives
	default:
		if ((cmd_byte & 0xC0) == 0x80)
		{
			// load vertices (use computed vertex size from FifoCommandRunnable above)
			u16 numVertices = DataReadU16();

			VertexLoaderManager::RunVertices(
				cmd_byte & GX_VAT_MASK,   // Vertex loader index (0 - 7)
				(cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
				numVertices);
		}
		else
		{
			ERROR_LOG(VIDEO, "OpcodeDecoding::Decode: Illegal command %02x", cmd_byte);
			break;
		}
		break;
	}

	// Display lists get added directly into the FIFO stream
	if (g_bRecordFifoData && cmd_byte != GX_CMD_CALL_DL)
		FifoRecorder::GetInstance().WriteGPCommand(opcodeStart, u32(g_pVideoData - opcodeStart));
}

static void DecodeSemiNop()
{
	u8 *opcodeStart = g_pVideoData;

	int cmd_byte = DataReadU8();
	switch (cmd_byte)
	{
	case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
	case GX_CMD_INVL_VC: // Invalidate Vertex Cache
	case GX_NOP:
		break;

	case GX_LOAD_CP_REG: //0x08
		// We have to let CP writes through because they determine the size of vertices.
		{
			u8 sub_cmd = DataReadU8();
			u32 value = DataReadU32();
			LoadCPReg(sub_cmd, value);
			INCSTAT(stats.thisFrame.numCPLoads);
		}
		break;

	case GX_LOAD_XF_REG:
		{
			u32 Cmd2 = DataReadU32();
			int transfer_size = ((Cmd2 >> 16) & 15) + 1;
			u32 address = Cmd2 & 0xFFFF;
			GC_ALIGNED128(u32 data_buffer[16]);
			DataReadU32xFuncs[transfer_size-1](data_buffer);
			LoadXFReg(transfer_size, address, data_buffer);
			INCSTAT(stats.thisFrame.numXFLoads);
		}
		break;

	case GX_LOAD_INDX_A: //used for position matrices
		LoadIndexedXF(DataReadU32(), 0xC);
		break;
	case GX_LOAD_INDX_B: //used for normal matrices
		LoadIndexedXF(DataReadU32(), 0xD);
		break;
	case GX_LOAD_INDX_C: //used for postmatrices
		LoadIndexedXF(DataReadU32(), 0xE);
		break;
	case GX_LOAD_INDX_D: //used for lights
		LoadIndexedXF(DataReadU32(), 0xF);
		break;

	case GX_CMD_CALL_DL:
		// Hm, wonder if any games put tokens in display lists - in that case,
		// we'll have to parse them too.
		DataSkip(8);
		break;

	case GX_LOAD_BP_REG: //0x61
		// We have to let BP writes through because they set tokens and stuff.
		// TODO: Call a much simplified LoadBPReg instead.
		{
			u32 bp_cmd = DataReadU32();
			LoadBPReg(bp_cmd);
			INCSTAT(stats.thisFrame.numBPLoads);
		}
		break;

	// draw primitives
	default:
		if ((cmd_byte & 0xC0) == 0x80)
		{
			// load vertices (use computed vertex size from FifoCommandRunnable above)
			u16 numVertices = DataReadU16();
			DataSkip(numVertices * VertexLoaderManager::GetVertexSize(cmd_byte & GX_VAT_MASK));
		}
		else
		{
			ERROR_LOG(VIDEO, "OpcodeDecoding::Decode: Illegal command %02x", cmd_byte);
			break;
		}
		break;
	}

	if (g_bRecordFifoData && cmd_byte != GX_CMD_CALL_DL)
		FifoRecorder::GetInstance().WriteGPCommand(opcodeStart, u32(g_pVideoData - opcodeStart));
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
	u32 totalCycles = 0;
	u32 cycles = FifoCommandRunnable();
	while (cycles > 0)
	{
		skipped_frame ? DecodeSemiNop() : Decode();
		totalCycles += cycles;
		cycles = FifoCommandRunnable();
	}
	return totalCycles;
}
