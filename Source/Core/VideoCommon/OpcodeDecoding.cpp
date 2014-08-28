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
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"


bool g_bRecordFifoData = false;

static u32 InterpretDisplayList(u32 address, u32 size)
{
	u8* old_pVideoData = g_video_buffer_read_ptr;
	u8* startAddress;

	if (g_use_deterministic_gpu_thread)
		startAddress = (u8*) PopFifoAuxBuffer(size);
	else
		startAddress = Memory::GetPointer(address);

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

static void InterpretDisplayListPreprocess(u32 address, u32 size)
{
	u8* old_read_ptr = g_video_buffer_pp_read_ptr;
	u8* startAddress = Memory::GetPointer(address);

	PushFifoAuxBuffer(startAddress, size);

	if (startAddress != nullptr)
	{
		g_video_buffer_pp_read_ptr = startAddress;

		u8 *end = startAddress + size;
		OpcodeDecoder_Preprocess(end);
	}

	g_video_buffer_pp_read_ptr = old_read_ptr;
}

static void UnknownOpcode(u8 cmd_byte, void *buffer, bool preprocess)
{
	// TODO(Omega): Maybe dump FIFO to file on this error
	std::string temp = StringFromFormat(
		"GFX FIFO: Unknown Opcode (0x%x @ %p, preprocessing=%s).\n"
		"This means one of the following:\n"
		"* The emulated GPU got desynced, disabling dual core can help\n"
		"* Command stream corrupted by some spurious memory bug\n"
		"* This really is an unknown opcode (unlikely)\n"
		"* Some other sort of bug\n\n"
		"Dolphin will now likely crash or hang. Enjoy." ,
		cmd_byte,
		buffer,
		preprocess ? "yes" : "no");
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

template <bool is_preprocess, u8** bufp>
static u32 Decode(u8* end)
{
	u8 *opcodeStart = *bufp;
	if (*bufp == end)
		return 0;

	u8 cmd_byte = DataRead<u8>(bufp);
	u32 cycles;
	int refarray;
	switch (cmd_byte)
	{
	case GX_NOP:
		cycles = 6; // Hm, this means that we scan over nop streams pretty slowly...
		break;

	case GX_LOAD_CP_REG: //0x08
		{
			if (end - *bufp < 1 + 4)
				return 0;
			cycles = 12;
			u8 sub_cmd = DataRead<u8>(bufp);
			u32 value = DataRead<u32>(bufp);
			LoadCPReg(sub_cmd, value, is_preprocess);
			if (!is_preprocess)
				INCSTAT(stats.thisFrame.numCPLoads);
		}
		break;

	case GX_LOAD_XF_REG:
		{
			if (end - *bufp < 4)
				return 0;
			u32 Cmd2 = DataRead<u32>(bufp);
			int transfer_size = ((Cmd2 >> 16) & 15) + 1;
			if ((size_t) (end - *bufp) < transfer_size * sizeof(u32))
				return 0;
			cycles = 18 + 6 * transfer_size;
			if (!is_preprocess)
			{
				u32 xf_address = Cmd2 & 0xFFFF;
				LoadXFReg(transfer_size, xf_address);

				INCSTAT(stats.thisFrame.numXFLoads);
			}
			else
			{
				*bufp += transfer_size * sizeof(u32);
			}
		}
		break;

	case GX_LOAD_INDX_A: //used for position matrices
		refarray = 0xC;
		goto load_indx;
	case GX_LOAD_INDX_B: //used for normal matrices
		refarray = 0xD;
		goto load_indx;
	case GX_LOAD_INDX_C: //used for postmatrices
		refarray = 0xE;
		goto load_indx;
	case GX_LOAD_INDX_D: //used for lights
		refarray = 0xF;
		goto load_indx;
	load_indx:
		if (end - *bufp < 4)
			return 0;
		cycles = 6;
		if (is_preprocess)
			PreprocessIndexedXF(DataRead<u32>(bufp), refarray);
		else
			LoadIndexedXF(DataRead<u32>(bufp), refarray);
		break;

	case GX_CMD_CALL_DL:
		{
			if (end - *bufp < 8)
				return 0;
			u32 address = DataRead<u32>(bufp);
			u32 count = DataRead<u32>(bufp);
			if (is_preprocess)
				InterpretDisplayListPreprocess(address, count);
			else
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
			if (end - *bufp < 4)
				return 0;
			cycles = 12;
			u32 bp_cmd = DataRead<u32>(bufp);
			if (is_preprocess)
			{
				LoadBPRegPreprocess(bp_cmd);
			}
			else
			{
				LoadBPReg(bp_cmd);
				INCSTAT(stats.thisFrame.numBPLoads);
			}
		}
		break;

	// draw primitives
	default:
		if ((cmd_byte & 0xC0) == 0x80)
		{
			cycles = 1600;
			// load vertices
			if (end - *bufp < 2)
				return 0;
			u16 num_vertices = DataRead<u16>(bufp);

			if (is_preprocess)
			{
				size_t size = num_vertices * VertexLoaderManager::GetVertexSize(cmd_byte & GX_VAT_MASK, is_preprocess);
				if ((size_t) (end - *bufp) < size)
					return 0;
				*bufp += size;
			}
			else
			{
				if (!VertexLoaderManager::RunVertices(
					cmd_byte & GX_VAT_MASK,   // Vertex loader index (0 - 7)
					(cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
					num_vertices,
					end - *bufp,
					g_bSkipCurrentFrame))
					return 0;
			}
		}
		else
		{
			UnknownOpcode(cmd_byte, opcodeStart, is_preprocess);
			cycles = 1;
		}
		break;
	}

	// Display lists get added directly into the FIFO stream
	if (!is_preprocess && g_bRecordFifoData && cmd_byte != GX_CMD_CALL_DL)
		FifoRecorder::GetInstance().WriteGPCommand(opcodeStart, u32(*bufp - opcodeStart));

	// In is_preprocess mode, we don't actually care about cycles, at least for
	// now... make sure the compiler realizes that.
	return is_preprocess ? 1 : cycles;
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
		u32 cycles = Decode</*is_preprocess*/ false, &g_video_buffer_read_ptr>(end);
		if (cycles == 0)
		{
			g_video_buffer_read_ptr = old;
			break;
		}
		totalCycles += cycles;
	}
	return totalCycles;
}

void OpcodeDecoder_Preprocess(u8 *end)
{
	while (true)
	{
		u8* old = g_video_buffer_pp_read_ptr;
		u32 cycles = Decode</*is_preprocess*/ true, &g_video_buffer_pp_read_ptr>(end);
		if (cycles == 0)
		{
			g_video_buffer_pp_read_ptr = old;
			break;
		}
	}
}
