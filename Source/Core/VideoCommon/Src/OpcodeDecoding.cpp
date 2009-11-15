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

//DL facts:
//	Ikaruga uses (nearly) NO display lists!
//  Zelda WW uses TONS of display lists
//  Zelda TP uses almost 100% display lists except menus (we like this!)
//  Super Mario Galaxy has nearly all geometry and more than half of the state in DLs (great!)

// Note that it IS NOT GENERALLY POSSIBLE to precompile display lists! You can compile them as they are
// while interpreting them, and hope that the vertex format doesn't change, though, if you do it right
// when they are called. The reason is that the vertex format affects the sizes of the vertices.

#include "Common.h"
#include "VideoCommon.h"
#include "Profiler.h"
#include "OpcodeDecoding.h"
#include "CommandProcessor.h"

#include "VertexLoaderManager.h"

#include "Statistics.h"

#include "XFMemory.h"
#include "CPMemory.h"
#include "BPMemory.h"

#include "Fifo.h"
#include "DataReader.h"

#include "OpenCL.h"
#if defined(HAVE_OPENCL) && HAVE_OPENCL
#include "OpenCL/OCLTextureDecoder.h"
#endif

u8* g_pVideoData = 0;

extern u8* FAKE_GetFifoStartPtr();
extern u8* FAKE_GetFifoEndPtr();

static void Decode();

void InterpretDisplayList(u32 address, u32 size)
{
	u8* old_pVideoData = g_pVideoData;
	u8* startAddress = Memory_GetPtr(address);

	// Avoid the crash if Memory_GetPtr failed ..
	if (startAddress != 0)
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

// Defer to plugin-specific DL cache.
extern bool HandleDisplayList(u32 address, u32 size);

void ExecuteDisplayList(u32 address, u32 size)
{
	if (!HandleDisplayList(address, size))
		InterpretDisplayList(address, size);
}

bool FifoCommandRunnable()
{
	u32 buffer_size = (u32)(FAKE_GetFifoEndPtr() - g_pVideoData);
    if (buffer_size == 0)
		return false;  // can't peek

	u8 cmd_byte = DataPeek8(0);	
    u32 command_size = 0;

    switch (cmd_byte)
    {
    case GX_NOP: // Hm, this means that we scan over nop streams pretty slowly...
	case GX_CMD_INVL_VC: // Invalidate Vertex Cache - no parameters
    case GX_CMD_UNKNOWN_METRICS: // zelda 4 swords calls it and checks the metrics registers after that
        command_size = 1;
        break;

	case GX_LOAD_BP_REG:	
        command_size = 5;
        break;

    case GX_LOAD_CP_REG:
        command_size = 6;
        break;

    case GX_LOAD_INDX_A:
    case GX_LOAD_INDX_B:
    case GX_LOAD_INDX_C:
    case GX_LOAD_INDX_D:
        command_size = 5;
        break;

    case GX_CMD_CALL_DL:	
        command_size = 9;
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
            }
            else
			{
                return false;
            }			
        }
        break;

    default:
        if (cmd_byte & 0x80)
        {				
            // check if we can read the header
            if (buffer_size >= 3)
			{
                command_size = 1 + 2;
                u16 numVertices = DataPeek16(1);
				command_size += numVertices * VertexLoaderManager::GetVertexSize(cmd_byte & GX_VAT_MASK);
            }
			else
			{				
                return false;
            }
        }
		else
		{
			// TODO(Omega): Maybe dump FIFO to file on this error
            char szTemp[1024];
            sprintf(szTemp, "GFX FIFO: Unknown Opcode (0x%x).\n"
				"This means one of the following:\n"
				"* The emulated GPU got desynced, disabling dual core can help\n"
				"* Command stream corrupted by some spurious memory bug\n"
				"* This really is an unknown opcode (unlikely)\n"
				"* Some other sort of bug\n\n"
				"Dolphin will now likely crash or hang. Enjoy." , cmd_byte);
            g_VideoInitialize.pSysMessage(szTemp);
            g_VideoInitialize.pLog(szTemp, TRUE);
			{
                SCPFifoStruct &fifo = CommandProcessor::fifo;

				char szTmp[256];
				// sprintf(szTmp, "Illegal command %02x (at %08x)",cmd_byte,g_pDataReader->GetPtr());
				sprintf(szTmp, "Illegal command %02x\n"
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
					"bFF_GPLinkEnable: %s\n"
					"bFF_Breakpoint: %s\n"
					,cmd_byte, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
					,fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint, fifo.bFF_GPReadEnable ? "true" : "false"
					,fifo.bFF_BPEnable ? "true" : "false" ,fifo.bFF_GPLinkEnable ? "true" : "false"
					,fifo.bFF_Breakpoint ? "true" : "false");

				g_VideoInitialize.pSysMessage(szTmp);
				g_VideoInitialize.pLog(szTmp, TRUE);
			}
        }
        break;
    }
    
    if (command_size > buffer_size)
        return false;

    // INFO_LOG("OP detected: cmd_byte 0x%x  size %i  buffer %i",cmd_byte, command_size, buffer_size);

    return true;
}

static void Decode()
{
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
            u32 address = Cmd2 & 0xFFFF;
			// TODO - speed this up. pshufb?
            u32 data_buffer[16];
            for (int i = 0; i < transfer_size; i++)
                data_buffer[i] = DataReadU32();
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
        {
            u32 address = DataReadU32();
            u32 count = DataReadU32();
            ExecuteDisplayList(address, count);
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
        if (cmd_byte & 0x80)
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
}

static void DecodeSemiNop()
{
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
			// TODO - speed this up. pshufb?
            u32 data_buffer[16];
            for (int i = 0; i < transfer_size; i++)
                data_buffer[i] = DataReadU32();
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
        if (cmd_byte & 0x80)
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
}

void OpcodeDecoder_Init()
{	
	g_pVideoData = FAKE_GetFifoStartPtr();

#if defined(HAVE_OPENCL) && HAVE_OPENCL
	OpenCL::Initialize();
	TexDecoder_OpenCL_Initialize();
#endif
}


void OpcodeDecoder_Shutdown()
{
#if defined(HAVE_OPENCL) && HAVE_OPENCL
	OpenCL::Destroy();
	TexDecoder_OpenCL_Shutdown();
#endif
}

void OpcodeDecoder_Run(bool skipped_frame)
{
	DVSTARTPROFILE();
	if (!skipped_frame)
	{
		while (FifoCommandRunnable())
			Decode();
	}
	else
	{
		while (FifoCommandRunnable())
			DecodeSemiNop();
	}
}
